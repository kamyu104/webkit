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

#ifndef WebInjectedScriptManager_h
#define WebInjectedScriptManager_h

#include "CommandLineAPIHost.h"
#include <inspector/InjectedScriptManager.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DOMWindow;

class WebInjectedScriptManager final : public Inspector::InjectedScriptManager {
public:
    WebInjectedScriptManager(Inspector::InspectorEnvironment&, RefPtr<Inspector::InjectedScriptHost>&&);
    virtual ~WebInjectedScriptManager() { }

    CommandLineAPIHost* commandLineAPIHost() const { return m_commandLineAPIHost.get(); }

    void disconnect() override;
    void discardInjectedScripts() override;

    void discardInjectedScriptsFor(DOMWindow*);

protected:
    void didCreateInjectedScript(const Inspector::InjectedScript&) override;

private:
    RefPtr<CommandLineAPIHost> m_commandLineAPIHost;
};

} // namespace WebCore

#endif // !defined(WebInjectedScriptManager_h)
