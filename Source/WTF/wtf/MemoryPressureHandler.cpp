/*
 * Copyright (C) 2011-2017 Apple Inc. All Rights Reserved.
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
#include "MemoryPressureHandler.h"

#include <wtf/MemoryFootprint.h>

#define LOG_CHANNEL_PREFIX Log

namespace WTF {

#if RELEASE_LOG_DISABLED
WTFLogChannel LogMemoryPressure = { WTFLogChannelOn, "MemoryPressure" };
#else
WTFLogChannel LogMemoryPressure = { WTFLogChannelOn, "MemoryPressure", LOG_CHANNEL_WEBKIT_SUBSYSTEM, OS_LOG_DEFAULT };
#endif

WTF_EXPORT_PRIVATE bool MemoryPressureHandler::ReliefLogger::s_loggingEnabled = false;

MemoryPressureHandler& MemoryPressureHandler::singleton()
{
    static NeverDestroyed<MemoryPressureHandler> memoryPressureHandler;
    return memoryPressureHandler;
}

MemoryPressureHandler::MemoryPressureHandler()
#if OS(LINUX)
    : m_holdOffTimer(RunLoop::main(), this, &MemoryPressureHandler::holdOffTimerFired)
#elif OS(WINDOWS)
    : m_windowsMeasurementTimer(RunLoop::main(), this, &MemoryPressureHandler::windowsMeasurementTimerFired)
#endif
{
}

void MemoryPressureHandler::setShouldUsePeriodicMemoryMonitor(bool use)
{
    if (!isFastMallocEnabled()) {
        // If we're running with FastMalloc disabled, some kind of testing or debugging is probably happening.
        // Let's be nice and not enable the memory kill mechanism.
        return;
    }

    if (use) {
        m_measurementTimer = std::make_unique<RunLoop::Timer<MemoryPressureHandler>>(RunLoop::main(), this, &MemoryPressureHandler::measurementTimerFired);
        m_measurementTimer->startRepeating(30);
    } else
        m_measurementTimer = nullptr;
}

#if !RELEASE_LOG_DISABLED
static const char* toString(MemoryUsagePolicy policy)
{
    switch (policy) {
    case MemoryUsagePolicy::Unrestricted: return "Unrestricted";
    case MemoryUsagePolicy::Conservative: return "Conservative";
    case MemoryUsagePolicy::Strict: return "Strict";
    }
}
#endif

static size_t thresholdForMemoryKillWithProcessState(WebsamProcessState processState)
{
#if CPU(X86_64) || CPU(ARM64)
    if (processState == WebsamProcessState::Active)
        return 4 * GB;
    return 2 * GB;
#else
    UNUSED_PARAM(processState);
    return 3 * GB;
#endif
}

size_t MemoryPressureHandler::thresholdForMemoryKill()
{
    return thresholdForMemoryKillWithProcessState(m_processState);
}

static size_t thresholdForPolicy(MemoryUsagePolicy policy)
{
    switch (policy) {
    case MemoryUsagePolicy::Conservative:
        return 1 * GB;
    case MemoryUsagePolicy::Strict:
        return 1.5 * GB;
    case MemoryUsagePolicy::Unrestricted:
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

static MemoryUsagePolicy policyForFootprint(size_t footprint)
{
    if (footprint >= thresholdForPolicy(MemoryUsagePolicy::Strict))
        return MemoryUsagePolicy::Strict;
    if (footprint >= thresholdForPolicy(MemoryUsagePolicy::Conservative))
        return MemoryUsagePolicy::Conservative;
    return MemoryUsagePolicy::Unrestricted;
}

void MemoryPressureHandler::shrinkOrDie()
{
    RELEASE_LOG(MemoryPressure, "Process is above the memory kill threshold. Trying to shrink down.");
    releaseMemory(Critical::Yes, Synchronous::Yes);

    auto footprint = memoryFootprint();
    RELEASE_ASSERT(footprint);
    RELEASE_LOG(MemoryPressure, "New memory footprint: %lu MB", footprint.value() / MB);

    if (footprint.value() < thresholdForMemoryKill()) {
        RELEASE_LOG(MemoryPressure, "Shrank below memory kill threshold. Process gets to live.");
        setMemoryUsagePolicyBasedOnFootprint(footprint.value());
        return;
    }

    RELEASE_ASSERT(m_memoryKillCallback);
    m_memoryKillCallback();
}

void MemoryPressureHandler::setMemoryUsagePolicyBasedOnFootprint(size_t footprint)
{
    auto newPolicy = policyForFootprint(footprint);
    if (newPolicy == m_memoryUsagePolicy)
        return;

    RELEASE_LOG(MemoryPressure, "Memory usage policy changed: %s -> %s", toString(m_memoryUsagePolicy), toString(newPolicy));
    m_memoryUsagePolicy = newPolicy;
    memoryPressureStatusChanged();
}

void MemoryPressureHandler::measurementTimerFired()
{
    auto footprint = memoryFootprint();
    if (!footprint)
        return;

    RELEASE_LOG(MemoryPressure, "Current memory footprint: %lu MB", footprint.value() / MB);
    if (footprint.value() >= thresholdForMemoryKill()) {
        shrinkOrDie();
        return;
    }

    setMemoryUsagePolicyBasedOnFootprint(footprint.value());

    switch (m_memoryUsagePolicy) {
    case MemoryUsagePolicy::Unrestricted:
        break;
    case MemoryUsagePolicy::Conservative:
        releaseMemory(Critical::No, Synchronous::No);
        break;
    case MemoryUsagePolicy::Strict:
        releaseMemory(Critical::Yes, Synchronous::No);
        break;
    }

    if (processState() == WebsamProcessState::Active && footprint.value() > thresholdForMemoryKillWithProcessState(WebsamProcessState::Inactive))
        doesExceedInactiveLimitWhileActive();
    else
        doesNotExceedInactiveLimitWhileActive();
}

void MemoryPressureHandler::doesExceedInactiveLimitWhileActive()
{
    if (m_hasInvokedDidExceedInactiveLimitWhileActiveCallback)
        return;
    if (m_didExceedInactiveLimitWhileActiveCallback)
        m_didExceedInactiveLimitWhileActiveCallback();
    m_hasInvokedDidExceedInactiveLimitWhileActiveCallback = true;
}

void MemoryPressureHandler::doesNotExceedInactiveLimitWhileActive()
{
    m_hasInvokedDidExceedInactiveLimitWhileActiveCallback = false;
}

void MemoryPressureHandler::setProcessState(WebsamProcessState state)
{
    if (m_processState == state)
        return;
    m_processState = state;
}

void MemoryPressureHandler::beginSimulatedMemoryPressure()
{
    if (m_isSimulatingMemoryPressure)
        return;
    m_isSimulatingMemoryPressure = true;
    memoryPressureStatusChanged();
    respondToMemoryPressure(Critical::Yes, Synchronous::Yes);
}

void MemoryPressureHandler::endSimulatedMemoryPressure()
{
    if (!m_isSimulatingMemoryPressure)
        return;
    m_isSimulatingMemoryPressure = false;
    memoryPressureStatusChanged();
}

void MemoryPressureHandler::releaseMemory(Critical critical, Synchronous synchronous)
{
    if (!m_lowMemoryHandler)
        return;

    ReliefLogger log("Total");
    m_lowMemoryHandler(critical, synchronous);
    platformReleaseMemory(critical);
}

void MemoryPressureHandler::setUnderMemoryPressure(bool underMemoryPressure)
{
    if (m_underMemoryPressure == underMemoryPressure)
        return;
    m_underMemoryPressure = underMemoryPressure;
    memoryPressureStatusChanged();
}

void MemoryPressureHandler::memoryPressureStatusChanged()
{
    if (m_memoryPressureStatusChangedCallback)
        m_memoryPressureStatusChangedCallback(isUnderMemoryPressure());
}

void MemoryPressureHandler::ReliefLogger::logMemoryUsageChange()
{
#if !RELEASE_LOG_DISABLED
#define STRING_SPECIFICATION "%{public}s"
#define MEMORYPRESSURE_LOG(...) RELEASE_LOG(MemoryPressure, __VA_ARGS__)
#else
#define STRING_SPECIFICATION "%s"
#define MEMORYPRESSURE_LOG(...) WTFLogAlways(__VA_ARGS__)
#endif

    auto currentMemory = platformMemoryUsage();
    if (!currentMemory || !m_initialMemory) {
        MEMORYPRESSURE_LOG("Memory pressure relief: " STRING_SPECIFICATION ": (Unable to get dirty memory information for process)", m_logString);
        return;
    }

    long residentDiff = currentMemory->resident - m_initialMemory->resident;
    long physicalDiff = currentMemory->physical - m_initialMemory->physical;

    MEMORYPRESSURE_LOG("Memory pressure relief: " STRING_SPECIFICATION ": res = %zu/%zu/%ld, res+swap = %zu/%zu/%ld",
        m_logString,
        m_initialMemory->resident, currentMemory->resident, residentDiff,
        m_initialMemory->physical, currentMemory->physical, physicalDiff);
}

#if !PLATFORM(COCOA) && !OS(LINUX) && !OS(WINDOWS)
void MemoryPressureHandler::install() { }
void MemoryPressureHandler::uninstall() { }
void MemoryPressureHandler::holdOff(unsigned) { }
void MemoryPressureHandler::respondToMemoryPressure(Critical, Synchronous) { }
void MemoryPressureHandler::platformReleaseMemory(Critical) { }
std::optional<MemoryPressureHandler::ReliefLogger::MemoryUsage> MemoryPressureHandler::ReliefLogger::platformMemoryUsage() { return std::nullopt; }
#endif

#if !OS(WINDOWS)
void MemoryPressureHandler::platformInitialize() { }
#endif

} // namespace WebCore
