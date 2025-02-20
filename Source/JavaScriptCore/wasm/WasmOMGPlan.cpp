/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WasmOMGPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "B3OpaqueByproducts.h"
#include "JSCInlines.h"
#include "JSWebAssemblyModule.h"
#include "LinkBuffer.h"
#include "WasmB3IRGenerator.h"
#include "WasmContext.h"
#include "WasmMachineThreads.h"
#include "WasmMemory.h"
#include "WasmValidate.h"
#include "WasmWorklist.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadMessage.h>
#include <wtf/text/StringBuilder.h>

namespace JSC { namespace Wasm {

static const bool verbose = false;

OMGPlan::OMGPlan(Ref<Module> module, uint32_t functionIndex, MemoryMode mode, CompletionTask&& task)
    : Base(nullptr, makeRef(const_cast<ModuleInformation&>(module->moduleInformation())), WTFMove(task))
    , m_module(module.copyRef())
    , m_codeBlock(*module->codeBlockFor(mode))
    , m_functionIndex(functionIndex)
{
    setMode(mode);
    ASSERT(m_codeBlock->runnable());
    ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(m_mode));
    dataLogLnIf(verbose, "Starting OMG plan for ", functionIndex, " of module: ", RawPointer(&m_module.get()));
}

void OMGPlan::work(CompilationEffort)
{
    ASSERT(m_codeBlock->runnable());
    ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(mode()));
    const FunctionLocationInBinary& location = m_moduleInformation->functionLocationInBinary[m_functionIndex];
    const uint8_t* functionStart = m_moduleInformation->source.data() + location.start;
    const size_t functionLength = location.end - location.start;
    ASSERT(functionStart + functionLength <= m_moduleInformation->source.end());

    const uint32_t functionIndexSpace = m_functionIndex + m_module->moduleInformation().importFunctionCount();
    ASSERT(functionIndexSpace < m_module->moduleInformation().functionIndexSpaceSize());

    SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[m_functionIndex];
    const Signature& signature = SignatureInformation::get(signatureIndex);
    ASSERT(validateFunction(functionStart, functionLength, signature, m_moduleInformation.get()));

    Vector<UnlinkedWasmToWasmCall> unlinkedCalls;
    CompilationContext context;
    auto parseAndCompileResult = parseAndCompile(context, functionStart, functionLength, signature, unlinkedCalls, m_moduleInformation.get(), m_mode, CompilationMode::OMGMode, m_functionIndex);

    if (UNLIKELY(!parseAndCompileResult)) {
        fail(holdLock(m_lock), makeString(parseAndCompileResult.error(), "when trying to tier up ", String::number(m_functionIndex)));
        return;
    }

    Entrypoint omgEntrypoint;
    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr);
    omgEntrypoint.compilation = std::make_unique<B3::Compilation>(
        FINALIZE_CODE(linkBuffer, ("WebAssembly OMG function[%i] %s", m_functionIndex, SignatureInformation::get(signatureIndex).toString().ascii().data())),
        WTFMove(context.wasmEntrypointByproducts));

    omgEntrypoint.calleeSaveRegisters = WTFMove(parseAndCompileResult.value()->entrypoint.calleeSaveRegisters);

    void* entrypoint;
    {
        ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(mode()));
        Ref<Callee> callee = Callee::create(WTFMove(omgEntrypoint), functionIndexSpace, m_moduleInformation->nameSection.get(functionIndexSpace));
        MacroAssembler::repatchPointer(parseAndCompileResult.value()->calleeMoveLocation, CalleeBits::boxWasm(callee.ptr()));
        ASSERT(!m_codeBlock->m_optimizedCallees[m_functionIndex]);
        entrypoint = callee->entrypoint();

        // We want to make sure we publish our callee at the same time as we link our callsites. This enables us to ensure we
        // always call the fastest code. Any function linked after us will see our new code and the new callsites, which they
        // will update. It's also ok if they publish their code before we reset the instruction caches because after we release
        // the lock our code is ready to be published too.
        LockHolder holder(m_codeBlock->m_lock);
        m_codeBlock->m_optimizedCallees[m_functionIndex] = WTFMove(callee);

        for (auto& call : unlinkedCalls) {
            void* entrypoint;
            if (call.functionIndexSpace < m_module->moduleInformation().importFunctionCount())
                entrypoint = m_codeBlock->m_wasmToWasmExitStubs[call.functionIndexSpace].code().executableAddress();
            else
                entrypoint = m_codeBlock->wasmEntrypointCalleeFromFunctionIndexSpace(call.functionIndexSpace).entrypoint();

            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel(entrypoint));
        }
        unlinkedCalls = std::exchange(m_codeBlock->m_wasmToWasmCallsites[m_functionIndex], unlinkedCalls);
    }

    // It's important to make sure we do this before we make any of the code we just compiled visible. If we didn't, we could end up
    // where we are tiering up some function A to A' and we repatch some function B to call A' instead of A. Another CPU could see
    // the updates to B but still not have reset its cache of A', which would lead to all kinds of badness.
    resetInstructionCacheOnAllThreads();
    WTF::storeStoreFence(); // This probably isn't necessary but it's good to be paranoid.

    m_codeBlock->m_wasmIndirectCallEntryPoints[m_functionIndex] = entrypoint;
    {
        LockHolder holder(m_codeBlock->m_lock);

        auto repatchCalls = [&] (const Vector<UnlinkedWasmToWasmCall>&  callsites) {
            for (auto& call : callsites) {
                dataLogLnIf(verbose, "Considering repatching call at: ", RawPointer(call.callLocation.dataLocation()), " that targets ", call.functionIndexSpace);
                if (call.functionIndexSpace == functionIndexSpace) {
                    dataLogLnIf(verbose, "Repatching call at: ", RawPointer(call.callLocation.dataLocation()), " to ", RawPointer(entrypoint));
                    MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel(entrypoint));
                }
            }

        };

        for (unsigned i = 0; i < m_codeBlock->m_wasmToWasmCallsites.size(); ++i) {
            if (i != functionIndexSpace)
                repatchCalls(m_codeBlock->m_wasmToWasmCallsites[i]);
        }

        // Make sure we repatch any recursive calls.
        repatchCalls(unlinkedCalls);
    }

    dataLogLnIf(verbose, "Finished with tier up count at: ", m_codeBlock->tierUpCount(m_functionIndex).count());
    complete(holdLock(m_lock));
}

void runOMGPlanForIndex(Context* context, uint32_t functionIndex)
{
    JSWebAssemblyCodeBlock* codeBlock = context->codeBlock();
    ASSERT(context->memoryMode() == codeBlock->m_codeBlock->mode());

    // We use the least significant bit of the tierUpCount to represent whether or not someone has already started the tier up.
    if (codeBlock->m_codeBlock->tierUpCount(functionIndex).shouldStartTierUp()) {
        Ref<Plan> plan = adoptRef(*new OMGPlan(context->module()->module(), functionIndex, codeBlock->m_codeBlock->mode(), Plan::dontFinalize()));
        ensureWorklist().enqueue(plan.copyRef());
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
