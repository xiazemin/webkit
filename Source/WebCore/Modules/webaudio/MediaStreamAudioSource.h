/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "AudioDestinationConsumer.h"
#include "RealtimeMediaSource.h"
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioBus;
class RealtimeMediaSourceCapabilities;

class MediaStreamAudioSource final : public RealtimeMediaSource {
public:
    static Ref<MediaStreamAudioSource> create();

    ~MediaStreamAudioSource() { }

    const RealtimeMediaSourceCapabilities& capabilities() const final;
    const RealtimeMediaSourceSettings& settings() const final;

    const String& deviceId() const { return m_deviceId; }
    void setDeviceId(const String& deviceId) { m_deviceId = deviceId; }

    void setAudioFormat(size_t numberOfChannels, float sampleRate);
    void consumeAudio(AudioBus*, size_t numberOfFrames);

    void addAudioConsumer(AudioDestinationConsumer*);
    bool removeAudioConsumer(AudioDestinationConsumer*);
    const Vector<RefPtr<AudioDestinationConsumer>>& audioConsumers() const { return m_audioConsumers; }

private:
    MediaStreamAudioSource();

    bool isCaptureSource() const final { return true; }

    String m_deviceId;
    Lock m_audioConsumersLock;
    Vector<RefPtr<AudioDestinationConsumer>> m_audioConsumers;
    RealtimeMediaSourceSettings m_currentSettings;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
