/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BufferSource.h"
#include "IDLTypes.h"
#include "JSDOMConvertBase.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWrapperCache.h"
#include "JSDynamicDowncast.h"
#include <runtime/JSTypedArrays.h>

namespace WebCore {

struct IDLInt8Array : IDLTypedArray<JSC::Int8Array> { };
struct IDLInt16Array : IDLTypedArray<JSC::Int16Array> { };
struct IDLInt32Array : IDLTypedArray<JSC::Int32Array> { };
struct IDLUint8Array : IDLTypedArray<JSC::Uint8Array> { };
struct IDLUint16Array : IDLTypedArray<JSC::Uint16Array> { };
struct IDLUint32Array : IDLTypedArray<JSC::Uint32Array> { };
struct IDLUint8ClampedArray : IDLTypedArray<JSC::Uint8ClampedArray> { };
struct IDLFloat32Array : IDLTypedArray<JSC::Float32Array> { };
struct IDLFloat64Array : IDLTypedArray<JSC::Float64Array> { };

inline RefPtr<JSC::Int8Array> toPossiblySharedInt8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int8Adaptor>(vm, value); }
inline RefPtr<JSC::Int16Array> toPossiblySharedInt16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int16Adaptor>(vm, value); }
inline RefPtr<JSC::Int32Array> toPossiblySharedInt32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int32Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8Array> toPossiblySharedUint8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint8Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8ClampedArray> toPossiblySharedUint8ClampedArray(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint8ClampedAdaptor>(vm, value); }
inline RefPtr<JSC::Uint16Array> toPossiblySharedUint16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint16Adaptor>(vm, value); }
inline RefPtr<JSC::Uint32Array> toPossiblySharedUint32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint32Adaptor>(vm, value); }
inline RefPtr<JSC::Float32Array> toPossiblySharedFloat32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Float32Adaptor>(vm, value); }
inline RefPtr<JSC::Float64Array> toPossiblySharedFloat64Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Float64Adaptor>(vm, value); }

inline RefPtr<JSC::Int8Array> toUnsharedInt8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int8Adaptor>(vm, value); }
inline RefPtr<JSC::Int16Array> toUnsharedInt16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int16Adaptor>(vm, value); }
inline RefPtr<JSC::Int32Array> toUnsharedInt32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int32Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8Array> toUnsharedUint8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint8Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8ClampedArray> toUnsharedUint8ClampedArray(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint8ClampedAdaptor>(vm, value); }
inline RefPtr<JSC::Uint16Array> toUnsharedUint16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint16Adaptor>(vm, value); }
inline RefPtr<JSC::Uint32Array> toUnsharedUint32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint32Adaptor>(vm, value); }
inline RefPtr<JSC::Float32Array> toUnsharedFloat32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Float32Adaptor>(vm, value); }
inline RefPtr<JSC::Float64Array> toUnsharedFloat64Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Float64Adaptor>(vm, value); }

inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, JSC::ArrayBuffer& buffer)
{
    if (auto result = getCachedWrapper(globalObject->world(), buffer))
        return result;

    // The JSArrayBuffer::create function will register the wrapper in finishCreation.
    return JSC::JSArrayBuffer::create(state->vm(), globalObject->arrayBufferStructure(buffer.sharingMode()), &buffer);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSC::JSGlobalObject* globalObject, JSC::ArrayBufferView& view)
{
    return view.wrap(state, globalObject);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, JSC::ArrayBuffer* buffer)
{
    if (!buffer)
        return JSC::jsNull();
    return toJS(state, globalObject, *buffer);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSC::JSGlobalObject* globalObject, JSC::ArrayBufferView* view)
{
    if (!view)
        return JSC::jsNull();
    return toJS(state, globalObject, *view);
}

inline RefPtr<JSC::ArrayBufferView> toPossiblySharedArrayBufferView(JSC::VM& vm, JSC::JSValue value)
{
    auto* wrapper = jsDynamicDowncast<JSC::JSArrayBufferView*>(vm, value);
    if (!wrapper)
        return nullptr;
    return wrapper->possiblySharedImpl();
}

inline RefPtr<JSC::ArrayBufferView> toUnsharedArrayBufferView(JSC::VM& vm, JSC::JSValue value)
{
    auto result = toPossiblySharedArrayBufferView(vm, value);
    if (!result || result->isShared())
        return nullptr;
    return result;
}

namespace Detail {

template<typename BufferSourceType>
struct BufferSourceConverter {
    using WrapperType = typename Converter<BufferSourceType>::WrapperType;
    using ReturnType = typename Converter<BufferSourceType>::ReturnType;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        ReturnType object = WrapperType::toWrapped(vm, value);
        if (UNLIKELY(!object))
            exceptionThrower(state, scope);
        return object;
    }
};

}

template<> struct Converter<IDLArrayBuffer> : DefaultConverter<IDLArrayBuffer> {
    using WrapperType = JSC::JSArrayBuffer;
    using ReturnType = JSC::ArrayBuffer*;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLArrayBuffer>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLArrayBuffer> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLDataView> : DefaultConverter<IDLDataView> {
    using WrapperType = JSC::JSDataView;
    using ReturnType = RefPtr<JSC::DataView>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLDataView>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLDataView> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLInt8Array> : DefaultConverter<IDLInt8Array> {
    using WrapperType = JSC::JSInt8Array;
    using ReturnType = RefPtr<JSC::Int8Array>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLInt8Array>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLInt8Array> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLInt16Array> : DefaultConverter<IDLInt16Array> {
    using WrapperType = JSC::JSInt16Array;
    using ReturnType = RefPtr<JSC::Int16Array>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLInt16Array>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLInt16Array> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLInt32Array> : DefaultConverter<IDLInt32Array> {
    using WrapperType = JSC::JSInt32Array;
    using ReturnType = RefPtr<JSC::Int32Array>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLInt32Array>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLInt32Array> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLUint8Array> : DefaultConverter<IDLUint8Array> {
    using WrapperType = JSC::JSUint8Array;
    using ReturnType = RefPtr<JSC::Uint8Array>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLUint8Array>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLUint8Array> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLUint16Array> : DefaultConverter<IDLUint16Array> {
    using WrapperType = JSC::JSUint16Array;
    using ReturnType = RefPtr<JSC::Uint16Array>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLUint16Array>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLUint16Array> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLUint32Array> : DefaultConverter<IDLUint32Array> {
    using WrapperType = JSC::JSUint32Array;
    using ReturnType = RefPtr<JSC::Uint32Array>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLUint32Array>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLUint32Array> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLUint8ClampedArray> : DefaultConverter<IDLUint8ClampedArray> {
    using WrapperType = JSC::JSUint8ClampedArray;
    using ReturnType = RefPtr<JSC::Uint8ClampedArray>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLUint8ClampedArray>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLUint8ClampedArray> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLFloat32Array> : DefaultConverter<IDLFloat32Array> {
    using WrapperType = JSC::JSFloat32Array;
    using ReturnType = RefPtr<JSC::Float32Array>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLFloat32Array>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLFloat32Array> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLFloat64Array> : DefaultConverter<IDLFloat64Array> {
    using WrapperType = JSC::JSFloat64Array;
    using ReturnType = RefPtr<JSC::Float64Array>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLFloat64Array>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLFloat64Array> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

template<> struct Converter<IDLArrayBufferView> : DefaultConverter<IDLArrayBufferView> {
    using WrapperType = JSC::JSArrayBufferView;
    using ReturnType = RefPtr<JSC::ArrayBufferView>;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        return Detail::BufferSourceConverter<IDLArrayBufferView>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<> struct JSConverter<IDLArrayBufferView> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }
};

} // namespace WebCore
