/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2016 Ericsson AB. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "JSRTCIceCandidate.h"

#include "Dictionary.h"
#include "ExceptionCode.h"
#include "JSDOMBinding.h"

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL constructJSRTCIceCandidate(ExecState* exec)
{
    ExceptionCode ec = 0;
    Dictionary sessionInit;
    if (exec->argumentCount() > 0) {
        sessionInit = Dictionary(exec, exec->argument(0));
        if (!sessionInit.isObject())
            return throwVMError(exec, createTypeError(exec, "Optional RTCIceCandidate constructor argument must be a valid Dictionary"));

        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    DOMConstructorObject* jsConstructor = jsCast<DOMConstructorObject*>(exec->callee());
    RefPtr<RTCIceCandidate> iceCandidate = RTCIceCandidate::create(sessionInit, ec);
    if (ec == TYPE_MISMATCH_ERR) {
        setDOMException(exec, ec);
        return throwVMError(exec, createTypeError(exec, "Invalid RTCIceCandidate constructor arguments"));
    }

    if (ec) {
        setDOMException(exec, ec);
        return throwVMError(exec, createTypeError(exec, "Error creating RTCIceCandidate"));
    }

    return JSValue::encode(CREATE_DOM_WRAPPER(jsConstructor->globalObject(), RTCIceCandidate, iceCandidate.get()));
}

JSValue JSRTCIceCandidate::sdpMid(ExecState& state) const
{
    return jsOwnedStringOrNull(&state, wrapped().sdpMid());
}

JSValue JSRTCIceCandidate::sdpMLineIndex(ExecState&) const
{
    Optional<unsigned short> index = wrapped().sdpMLineIndex();
    return index ? JSValue(index.value()) : jsNull();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

