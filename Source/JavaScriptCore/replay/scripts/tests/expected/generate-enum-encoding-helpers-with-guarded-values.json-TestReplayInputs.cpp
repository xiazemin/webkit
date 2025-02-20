/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// DO NOT EDIT THIS FILE. It is automatically generated from generate-enum-encoding-helpers-with-guarded-values.json
// by the script: JavaScriptCore/replay/scripts/CodeGeneratorReplayInputs.py

#include "config.h"
#include "generate-enum-encoding-helpers-with-guarded-values.json-TestReplayInputs.h"

#if ENABLE(WEB_REPLAY)
#include "InternalNamespaceImplIncludeDummy.h"
#include <platform/ExternalNamespaceImplIncludeDummy.h>
#include <platform/PlatformMouseEvent.h>

namespace Test {
SavedMouseButton::SavedMouseButton(MouseButton button)
    : NondeterministicInput<SavedMouseButton>()
    , m_button(button)
{
}

SavedMouseButton::~SavedMouseButton()
{
}
} // namespace Test

namespace JSC {
const String& InputTraits<Test::SavedMouseButton>::type()
{
    static NeverDestroyed<const String> type(MAKE_STATIC_STRING_IMPL("SavedMouseButton"));
    return type;
}

void InputTraits<Test::SavedMouseButton>::encode(EncodedValue& encodedValue, const Test::SavedMouseButton& input)
{
    encodedValue.put<WebCore::MouseButton>(ASCIILiteral("button"), input.button());
}

bool InputTraits<Test::SavedMouseButton>::decode(EncodedValue& encodedValue, std::unique_ptr<Test::SavedMouseButton>& input)
{
    WebCore::MouseButton button;
    if (!encodedValue.get<WebCore::MouseButton>(ASCIILiteral("button"), button))
        return false;

    input = std::make_unique<Test::SavedMouseButton>(button);
    return true;
}

} // namespace JSC

#endif // ENABLE(WEB_REPLAY)
