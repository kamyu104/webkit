/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef JSGlobalObjectConsoleClient_h
#define JSGlobalObjectConsoleClient_h

#include "ConsoleClient.h"

namespace Inspector {

class InspectorConsoleAgent;

class JSGlobalObjectConsoleClient final : public JSC::ConsoleClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit JSGlobalObjectConsoleClient(InspectorConsoleAgent*);
    virtual ~JSGlobalObjectConsoleClient() { }

    static bool logToSystemConsole();
    static void setLogToSystemConsole(bool);

protected:
    void messageWithTypeAndLevel(MessageType, MessageLevel, JSC::ExecState*, RefPtr<ScriptArguments>&&) override;
    void count(JSC::ExecState*, RefPtr<ScriptArguments>&&) override;
    void profile(JSC::ExecState*, const String& title) override;
    void profileEnd(JSC::ExecState*, const String& title) override;
    void time(JSC::ExecState*, const String& title) override;
    void timeEnd(JSC::ExecState*, const String& title) override;
    void timeStamp(JSC::ExecState*, RefPtr<ScriptArguments>&&) override;

private:
    void warnUnimplemented(const String& method);
    void internalAddMessage(MessageType, MessageLevel, JSC::ExecState*, RefPtr<ScriptArguments>&&);

    InspectorConsoleAgent* m_consoleAgent;
};

}

#endif // !defined(JSGlobalObjectConsoleClient_h)
