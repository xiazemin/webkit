/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "WasmBBQPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "WasmB3IRGenerator.h"
#include "WasmBinding.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmFaultSignalHandler.h"
#include "WasmMemory.h"
#include "WasmModuleParser.h"
#include "WasmTierUpCount.h"
#include "WasmValidate.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/StringBuilder.h>

namespace JSC { namespace Wasm {

static const bool verbose = false;

BBQPlan::BBQPlan(VM* vm, Ref<ModuleInformation> info, AsyncWork work, CompletionTask&& task)
    : Base(vm, WTFMove(info), WTFMove(task))
    , m_state(State::Validated)
    , m_asyncWork(work)
{
}

BBQPlan::BBQPlan(VM* vm, Vector<uint8_t>&& source, AsyncWork work, CompletionTask&& task)
    : BBQPlan(vm, makeRef(*new ModuleInformation(WTFMove(source))), work, WTFMove(task))
{
    m_state = State::Initial;
}

BBQPlan::BBQPlan(VM* vm, const uint8_t* source, size_t sourceLength, AsyncWork work, CompletionTask&& task)
    : Base(vm, source, sourceLength, WTFMove(task))
    , m_state(State::Initial)
    , m_asyncWork(work)
{
}

const char* BBQPlan::stateString(State state)
{
    switch (state) {
    case State::Initial: return "Initial";
    case State::Validated: return "Validated";
    case State::Prepared: return "Prepared";
    case State::Compiled: return "Compiled";
    case State::Completed: return "Completed";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void BBQPlan::moveToState(State state)
{
    ASSERT(state >= m_state);
    dataLogLnIf(verbose && state != m_state, "moving to state: ", stateString(state), " from state: ", stateString(m_state));
    m_state = state;
}

bool BBQPlan::parseAndValidateModule()
{
    if (m_state != State::Initial)
        return true;

    dataLogLnIf(verbose, "starting validation");
    MonotonicTime startTime;
    if (verbose || Options::reportCompileTimes())
        startTime = MonotonicTime::now();

    {
        ModuleParser moduleParser(m_source, m_sourceLength, m_moduleInformation);
        auto parseResult = moduleParser.parse();
        if (!parseResult) {
            Base::fail(holdLock(m_lock), WTFMove(parseResult.error()));
            return false;
        }
    }

    const auto& functionLocations = m_moduleInformation->functionLocationInBinary;
    for (unsigned functionIndex = 0; functionIndex < functionLocations.size(); ++functionIndex) {
        dataLogLnIf(verbose, "Processing function starting at: ", functionLocations[functionIndex].start, " and ending at: ", functionLocations[functionIndex].end);
        const uint8_t* functionStart = m_source + functionLocations[functionIndex].start;
        size_t functionLength = functionLocations[functionIndex].end - functionLocations[functionIndex].start;
        ASSERT(functionLength <= m_sourceLength);
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature& signature = SignatureInformation::get(signatureIndex);

        auto validationResult = validateFunction(functionStart, functionLength, signature, m_moduleInformation.get());
        if (!validationResult) {
            if (verbose) {
                for (unsigned i = 0; i < functionLength; ++i)
                    dataLog(RawPointer(reinterpret_cast<void*>(functionStart[i])), ", ");
                dataLogLn();
            }
            Base::fail(holdLock(m_lock), makeString(validationResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
            return false;
        }
    }

    if (verbose || Options::reportCompileTimes())
        dataLogLn("Took ", (MonotonicTime::now() - startTime).microseconds(), " us to validate module");

    moveToState(State::Validated);
    if (m_asyncWork == Validation)
        complete(holdLock(m_lock));
    return true;
}

void BBQPlan::prepare()
{
    ASSERT(m_state == State::Validated);
    dataLogLnIf(verbose, "Starting preparation");

    auto tryReserveCapacity = [this] (auto& vector, size_t size, const char* what) {
        if (UNLIKELY(!vector.tryReserveCapacity(size))) {
            StringBuilder builder;
            builder.appendLiteral("Failed allocating enough space for ");
            builder.appendNumber(size);
            builder.append(what);
            fail(holdLock(m_lock), builder.toString());
            return false;
        }
        return true;
    };

    const auto& functionLocations = m_moduleInformation->functionLocationInBinary;
    if (!tryReserveCapacity(m_wasmToWasmExitStubs, m_moduleInformation->importFunctionSignatureIndices.size(), " WebAssembly to JavaScript stubs")
        || !tryReserveCapacity(m_unlinkedWasmToWasmCalls, functionLocations.size(), " unlinked WebAssembly to WebAssembly calls")
        || !tryReserveCapacity(m_wasmInternalFunctions, functionLocations.size(), " WebAssembly functions")
        || !tryReserveCapacity(m_compilationContexts, functionLocations.size(), " compilation contexts")
        || !tryReserveCapacity(m_tierUpCounts, functionLocations.size(), " tier-up counts"))
        return;

    m_unlinkedWasmToWasmCalls.resize(functionLocations.size());
    m_wasmInternalFunctions.resize(functionLocations.size());
    m_compilationContexts.resize(functionLocations.size());
    m_tierUpCounts.resize(functionLocations.size());

    for (unsigned importIndex = 0; importIndex < m_moduleInformation->imports.size(); ++importIndex) {
        Import* import = &m_moduleInformation->imports[importIndex];
        if (import->kind != ExternalKind::Function)
            continue;
        unsigned importFunctionIndex = m_wasmToWasmExitStubs.size();
        dataLogLnIf(verbose, "Processing import function number ", importFunctionIndex, ": ", makeString(import->module), ": ", makeString(import->field));
        m_wasmToWasmExitStubs.uncheckedAppend(wasmToWasm(importFunctionIndex));
    }

    const uint32_t importFunctionCount = m_moduleInformation->importFunctionCount();
    for (const auto& exp : m_moduleInformation->exports) {
        if (exp.kindIndex >= importFunctionCount)
            m_exportedFunctionIndices.add(exp.kindIndex - importFunctionCount);
    }

    for (const auto& element : m_moduleInformation->elements) {
        for (const uint32_t elementIndex : element.functionIndices) {
            if (elementIndex >= importFunctionCount)
                m_exportedFunctionIndices.add(elementIndex - importFunctionCount);
        }
    }

    if (m_moduleInformation->startFunctionIndexSpace && m_moduleInformation->startFunctionIndexSpace >= importFunctionCount)
        m_exportedFunctionIndices.add(*m_moduleInformation->startFunctionIndexSpace - importFunctionCount);

    moveToState(State::Prepared);
}

// We don't have a semaphore class... and this does kinda interesting things.
class BBQPlan::ThreadCountHolder {
public:
    ThreadCountHolder(BBQPlan& plan)
        : m_plan(plan)
    {
        LockHolder locker(m_plan.m_lock);
        m_plan.m_numberOfActiveThreads++;
    }

    ~ThreadCountHolder()
    {
        LockHolder locker(m_plan.m_lock);
        m_plan.m_numberOfActiveThreads--;

        if (!m_plan.m_numberOfActiveThreads && !m_plan.hasWork())
            m_plan.complete(locker);
    }

    BBQPlan& m_plan;
};

void BBQPlan::compileFunctions(CompilationEffort effort)
{
    ASSERT(m_state >= State::Prepared);
    dataLogLnIf(verbose, "Starting compilation");

    if (!hasWork())
        return;

    TraceScope traceScope(WebAssemblyCompileStart, WebAssemblyCompileEnd);
    ThreadCountHolder holder(*this);

    size_t bytesCompiled = 0;
    const auto& functionLocations = m_moduleInformation->functionLocationInBinary;
    while (true) {
        if (effort == Partial && bytesCompiled >= Options::webAssemblyPartialCompileLimit())
            return;

        uint32_t functionIndex;
        {
            auto locker = holdLock(m_lock);
            if (m_currentIndex >= functionLocations.size()) {
                if (hasWork())
                    moveToState(State::Compiled);
                return;
            }
            functionIndex = m_currentIndex;
            ++m_currentIndex;
        }

        const uint8_t* functionStart = m_source + functionLocations[functionIndex].start;
        size_t functionLength = functionLocations[functionIndex].end - functionLocations[functionIndex].start;
        ASSERT(functionLength <= m_sourceLength);
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature& signature = SignatureInformation::get(signatureIndex);
        unsigned functionIndexSpace = m_wasmToWasmExitStubs.size() + functionIndex;
        ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace) == signatureIndex);
        ASSERT(validateFunction(functionStart, functionLength, signature, m_moduleInformation.get()));

        m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();
        TierUpCount* tierUp = Options::useBBQTierUpChecks() ? &m_tierUpCounts[functionIndex] : nullptr;
        auto parseAndCompileResult = parseAndCompile(m_compilationContexts[functionIndex], functionStart, functionLength, signature, m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), m_mode, CompilationMode::BBQMode, functionIndex, tierUp);

        if (UNLIKELY(!parseAndCompileResult)) {
            auto locker = holdLock(m_lock);
            if (!m_errorMessage) {
                // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
                fail(locker, makeString(parseAndCompileResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
            }
            m_currentIndex = functionLocations.size();
            return;
        }

        m_wasmInternalFunctions[functionIndex] = WTFMove(*parseAndCompileResult);

        if (m_exportedFunctionIndices.contains(functionIndex)) {
            auto locker = holdLock(m_lock);
            auto result = m_jsToWasmInternalFunctions.add(functionIndex, createJSToWasmWrapper(m_compilationContexts[functionIndex], signature, &m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), m_mode, functionIndex));
            ASSERT_UNUSED(result, result.isNewEntry);
        }

        bytesCompiled += functionLength;
    }
}

void BBQPlan::complete(const AbstractLocker& locker)
{
    ASSERT(m_state != State::Compiled || m_currentIndex >= m_moduleInformation->functionLocationInBinary.size());
    dataLogLnIf(verbose, "Starting Completion");

    if (m_state == State::Compiled) {
        for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functionLocationInBinary.size(); functionIndex++) {
            CompilationContext& context = m_compilationContexts[functionIndex];
            SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
            {
                LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr);
                m_wasmInternalFunctions[functionIndex]->entrypoint.compilation = std::make_unique<B3::Compilation>(
                    FINALIZE_CODE(linkBuffer, ("WebAssembly function[%i] %s", functionIndex, SignatureInformation::get(signatureIndex).toString().ascii().data())),
                    WTFMove(context.wasmEntrypointByproducts));
            }

            if (auto jsToWasmInternalFunction = m_jsToWasmInternalFunctions.get(functionIndex)) {
                LinkBuffer linkBuffer(*context.jsEntrypointJIT, nullptr);
                jsToWasmInternalFunction->entrypoint.compilation = std::make_unique<B3::Compilation>(
                    FINALIZE_CODE(linkBuffer, ("JavaScript->WebAssembly entrypoint[%i] %s", functionIndex, SignatureInformation::get(signatureIndex).toString().ascii().data())),
                    WTFMove(context.jsEntrypointByproducts));
            }
        }

        for (auto& unlinked : m_unlinkedWasmToWasmCalls) {
            for (auto& call : unlinked) {
                void* executableAddress;
                if (m_moduleInformation->isImportedFunctionFromFunctionIndexSpace(call.functionIndexSpace)) {
                    // FIXME imports could have been linked in B3, instead of generating a patchpoint. This condition should be replaced by a RELEASE_ASSERT. https://bugs.webkit.org/show_bug.cgi?id=166462
                    executableAddress = m_wasmToWasmExitStubs.at(call.functionIndexSpace).code().executableAddress();
                } else
                    executableAddress = m_wasmInternalFunctions.at(call.functionIndexSpace - m_moduleInformation->importFunctionCount())->entrypoint.compilation->code().executableAddress();
                MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel(executableAddress));
            }
        }
    }
    
    if (!isComplete()) {
        moveToState(State::Completed);
        runCompletionTasks(locker);
    }
}

void BBQPlan::work(CompilationEffort effort)
{
    switch (m_state) {
    case State::Initial:
        parseAndValidateModule();
        if (!hasWork()) {
            ASSERT(isComplete());
            return;
        }
        FALLTHROUGH;
    case State::Validated:
        prepare();
        return;
    case State::Prepared:
        compileFunctions(effort);
        return;
    default:
        break;
    }
    return;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
