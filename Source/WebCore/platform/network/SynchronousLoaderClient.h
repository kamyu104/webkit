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

#ifndef SynchronousLoaderClient_h
#define SynchronousLoaderClient_h

#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"

namespace WebCore {

class SynchronousLoaderClient final : public ResourceHandleClient {
public:
    virtual ~SynchronousLoaderClient();

    void setAllowStoredCredentials(bool allow) { m_allowStoredCredentials = allow; }
    const ResourceResponse& response() const { return m_response; }
    Vector<char>& mutableData() { return m_data; }
    const ResourceError& error() const { return m_error; }
    bool isDone() { return m_isDone; }

    WEBCORE_EXPORT static ResourceError platformBadResponseError();

private:
    void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse& /*redirectResponse*/) override;
    bool shouldUseCredentialStorage(ResourceHandle*) override;
    void didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&) override;
    void didReceiveResponse(ResourceHandle*, const ResourceResponse&) override;
    void didReceiveData(ResourceHandle*, const char*, unsigned, int /*encodedDataLength*/) override;
    void didFinishLoading(ResourceHandle*, double /*finishTime*/) override;
    void didFail(ResourceHandle*, const ResourceError&) override;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(ResourceHandle*, const ProtectionSpace&) override;
#endif

    bool m_allowStoredCredentials { false };
    ResourceResponse m_response;
    Vector<char> m_data;
    ResourceError m_error;
    bool m_isDone { false };
};
}

#endif // SynchronousLoaderClient_h
