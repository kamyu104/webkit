/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchBody.h"

#if ENABLE(FETCH_API)

#include "DOMRequestState.h"
#include "Dictionary.h"
#include "ExceptionCode.h"
#include "FetchBodyOwner.h"
#include "HTTPParsers.h"
#include "JSBlob.h"
#include "JSDOMFormData.h"

namespace WebCore {

FetchBody::FetchBody(Ref<Blob>&& blob)
    : m_type(Type::Blob)
    , m_mimeType(blob->type())
    , m_blob(WTFMove(blob))
{
}

FetchBody::FetchBody(Ref<DOMFormData>&& formData)
    : m_type(Type::FormData)
    , m_mimeType(ASCIILiteral("multipart/form-data"))
    , m_formData(WTFMove(formData))
{
    // FIXME: Handle the boundary parameter of multipart/form-data MIME type.
}

FetchBody::FetchBody(String&& text)
    : m_type(Type::Text)
    , m_mimeType(ASCIILiteral("text/plain;charset=UTF-8"))
    , m_text(WTFMove(text))
{
}

FetchBody FetchBody::extract(JSC::ExecState& state, JSC::JSValue value)
{
    if (value.inherits(JSBlob::info()))
        return FetchBody(*JSBlob::toWrapped(value));
    if (value.inherits(JSDOMFormData::info()))
        return FetchBody(*JSDOMFormData::toWrapped(value));
    if (value.isString())
        return FetchBody(value.toWTFString(&state));
    return { };
}

FetchBody FetchBody::extractFromBody(FetchBody* body)
{
    if (!body)
        return { };

    body->m_isDisturbed = true;
    return FetchBody(WTFMove(*body));
}

bool FetchBody::processIfEmptyOrDisturbed(Consumer::Type type, DeferredWrapper& promise)
{
    if (m_type == Type::None) {
        switch (type) {
        case Consumer::Type::Text:
            promise.resolve(String());
            return true;
        case Consumer::Type::Blob:
            promise.resolve<RefPtr<Blob>>(Blob::create());
            return true;
        case Consumer::Type::JSON:
            promise.reject<ExceptionCode>(SYNTAX_ERR);
            return true;
        case Consumer::Type::ArrayBuffer:
            promise.resolve(ArrayBuffer::create(nullptr, 0));
            return true;
        default:
            ASSERT_NOT_REACHED();
            promise.reject<ExceptionCode>(0);
            return true;
        };
    }

    if (m_isDisturbed) {
        promise.reject<ExceptionCode>(TypeError);
        return true;
    }
    m_isDisturbed = true;
    return false;
}

void FetchBody::arrayBuffer(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    if (processIfEmptyOrDisturbed(Consumer::Type::ArrayBuffer, promise))
        return;
    consume(owner, Consumer::Type::ArrayBuffer, WTFMove(promise));
}

void FetchBody::blob(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    if (processIfEmptyOrDisturbed(Consumer::Type::Blob, promise))
        return;

    consume(owner, Consumer::Type::Blob, WTFMove(promise));
}

void FetchBody::json(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    if (processIfEmptyOrDisturbed(Consumer::Type::JSON, promise))
        return;

    if (m_type == Type::Text) {
        fulfillPromiseWithJSON(promise, m_text);
        return;
    }
    consume(owner, Consumer::Type::JSON, WTFMove(promise));
}

void FetchBody::text(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    if (processIfEmptyOrDisturbed(Consumer::Type::Text, promise))
        return;

    if (m_type == Type::Text) {
        promise.resolve(m_text);
        return;
    }
    consume(owner, Consumer::Type::Text, WTFMove(promise));
}

void FetchBody::consume(FetchBodyOwner& owner, Consumer::Type type, DeferredWrapper&& promise)
{
    if (m_type == Type::Text) {
        consumeText(type, WTFMove(promise));
        return;
    }
    if (m_type == Type::Blob) {
        consumeBlob(owner, type, WTFMove(promise));
        return;
    }

    // FIXME: Support other types.
    promise.reject<ExceptionCode>(0);
}

void FetchBody::consumeText(Consumer::Type type, DeferredWrapper&& promise)
{
    ASSERT(type == Consumer::Type::ArrayBuffer || type == Consumer::Type::Blob);

    if (type == Consumer::Type::ArrayBuffer) {
        Vector<char> data = extractFromText();
        promise.resolve<RefPtr<ArrayBuffer>>(ArrayBuffer::create(data.data(), data.size()));
        return;
    }
    String contentType = Blob::normalizedContentType(extractMIMETypeFromMediaType(m_mimeType));
    promise.resolve<RefPtr<Blob>>(Blob::create(extractFromText(), contentType));
}

FetchLoader::Type FetchBody::loadingType(Consumer::Type type)
{
    switch (type) {
    case Consumer::Type::JSON:
    case Consumer::Type::Text:
        return FetchLoader::Type::Text;
    case Consumer::Type::Blob:
    case Consumer::Type::ArrayBuffer:
        return FetchLoader::Type::ArrayBuffer;
    default:
        ASSERT_NOT_REACHED();
        return FetchLoader::Type::ArrayBuffer;
    };
}

void FetchBody::consumeBlob(FetchBodyOwner& owner, Consumer::Type type, DeferredWrapper&& promise)
{
    ASSERT(m_blob);

    m_consumer = Consumer({type, WTFMove(promise)});
    owner.loadBlob(*m_blob, loadingType(type));
}

Vector<char> FetchBody::extractFromText() const
{
    ASSERT(m_type == Type::Text);
    // FIXME: This double allocation is not efficient. Might want to fix that at WTFString level.
    CString data = m_text.utf8();
    Vector<char> value(data.length());
    memcpy(value.data(), data.data(), data.length());
    return value;
}

void FetchBody::loadingFailed()
{
    ASSERT(m_consumer);
    m_consumer->promise.reject<ExceptionCode>(0);
    m_consumer = Nullopt;
}

void FetchBody::loadedAsArrayBuffer(RefPtr<ArrayBuffer>&& buffer)
{
    ASSERT(m_consumer);
    ASSERT(m_consumer->type == Consumer::Type::Blob || m_consumer->type == Consumer::Type::ArrayBuffer);
    if (m_consumer->type == Consumer::Type::ArrayBuffer)
        m_consumer->promise.resolve(buffer);
    else {
        ASSERT(m_blob);
        Vector<char> data;
        data.reserveCapacity(buffer->byteLength());
        data.append(static_cast<const char*>(buffer->data()), buffer->byteLength());
        m_consumer->promise.resolve<RefPtr<Blob>>(Blob::create(WTFMove(data), m_blob->type()));
    }
    m_consumer = Nullopt;
}

void FetchBody::loadedAsText(String&& text)
{
    ASSERT(m_consumer);
    ASSERT(m_consumer->type == Consumer::Type::Text || m_consumer->type == Consumer::Type::JSON);
    if (m_consumer->type == Consumer::Type::Text)
        m_consumer->promise.resolve(text);
    else
        fulfillPromiseWithJSON(m_consumer->promise, text);
    m_consumer = Nullopt;
}

}

#endif // ENABLE(FETCH_API)
