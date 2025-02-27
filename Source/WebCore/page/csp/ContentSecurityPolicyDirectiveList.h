/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#ifndef ContentSecurityPolicyDirectiveList_h
#define ContentSecurityPolicyDirectiveList_h

#include "ContentSecurityPolicy.h"
#include "ContentSecurityPolicyHash.h"
#include "ContentSecurityPolicyMediaListDirective.h"
#include "ContentSecurityPolicySourceListDirective.h"
#include "URL.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

bool isCSPDirectiveName(const String&);

class ContentSecurityPolicyDirectiveList {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ContentSecurityPolicyDirectiveList)
public:
    static std::unique_ptr<ContentSecurityPolicyDirectiveList> create(ContentSecurityPolicy&, const String&, ContentSecurityPolicyHeaderType, ContentSecurityPolicy::PolicyFrom);
    ContentSecurityPolicyDirectiveList(ContentSecurityPolicy&, ContentSecurityPolicyHeaderType);

    const String& header() const { return m_header; }
    ContentSecurityPolicyHeaderType headerType() const { return m_headerType; }

    enum class ReportingStatus {
        SendReport,
        SuppressReport
    };
    bool allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus) const;
    bool allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus) const;
    bool allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus) const;
    bool allowInlineScriptWithHash(const ContentSecurityPolicyHash&) const;
    bool allowScriptWithNonce(const String& nonce) const;
    bool allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus) const;
    bool allowInlineStyleWithHash(const ContentSecurityPolicyHash&) const;
    bool allowStyleWithNonce(const String& nonce) const;
    bool allowEval(JSC::ExecState*, ReportingStatus) const;
    bool allowPluginType(const String& type, const String& typeAttribute, const URL&, ReportingStatus) const;

    bool allowScriptFromSource(const URL&, ReportingStatus) const;
    bool allowObjectFromSource(const URL&, ReportingStatus) const;
    bool allowChildFrameFromSource(const URL&, ReportingStatus) const;
    bool allowChildContextFromSource(const URL&, ReportingStatus) const;
    bool allowImageFromSource(const URL&, ReportingStatus) const;
    bool allowStyleFromSource(const URL&, ReportingStatus) const;
    bool allowFontFromSource(const URL&, ReportingStatus) const;
    bool allowMediaFromSource(const URL&, ReportingStatus) const;
    bool allowConnectToSource(const URL&, ReportingStatus) const;
    bool allowFormAction(const URL&, ReportingStatus) const;
    bool allowBaseURI(const URL&, ReportingStatus) const;

    bool allowFrameAncestors(const Frame&, const URL&, ReportingStatus) const;

    const String& evalDisabledErrorMessage() const { return m_evalDisabledErrorMessage; }
    ContentSecurityPolicy::ReflectedXSSDisposition reflectedXSSDisposition() const { return m_reflectedXSSDisposition; }
    bool isReportOnly() const { return m_reportOnly; }
    const Vector<String>& reportURIs() const { return m_reportURIs; }

private:
    void parse(const String&, ContentSecurityPolicy::PolicyFrom);

    bool parseDirective(const UChar* begin, const UChar* end, String& name, String& value);
    void parseReportURI(const String& name, const String& value);
    void parsePluginTypes(const String& name, const String& value);
    void parseReflectedXSS(const String& name, const String& value);
    void addDirective(const String& name, const String& value);
    void applySandboxPolicy(const String& name, const String& sandboxPolicy);

    template <class CSPDirectiveType>
    void setCSPDirective(const String& name, const String& value, std::unique_ptr<CSPDirectiveType>&);

    ContentSecurityPolicySourceListDirective* operativeDirective(ContentSecurityPolicySourceListDirective*) const;
    void reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const URL& blockedURL = URL(), const String& contextURL = String(), const WTF::OrdinalNumber& contextLine = WTF::OrdinalNumber::beforeFirst(), JSC::ExecState* = nullptr) const;

    void setEvalDisabledErrorMessage(const String& errorMessage) { m_evalDisabledErrorMessage = errorMessage; }

    bool checkEvalAndReportViolation(ContentSecurityPolicySourceListDirective*, const String& consoleMessage, const String& contextURL = String(), const WTF::OrdinalNumber& contextLine = WTF::OrdinalNumber::beforeFirst(), JSC::ExecState* = nullptr) const;
    bool checkInlineAndReportViolation(ContentSecurityPolicySourceListDirective*, const String& consoleMessage, const String& contextURL, const WTF::OrdinalNumber& contextLine, bool isScript) const;

    bool checkSourceAndReportViolation(ContentSecurityPolicySourceListDirective*, const URL&, const String& effectiveDirective) const;
    bool checkFrameAncestorsAndReportViolation(ContentSecurityPolicySourceListDirective*, const Frame&, const URL&, const String& effectiveDirective) const;
    bool checkMediaTypeAndReportViolation(ContentSecurityPolicyMediaListDirective*, const String& type, const String& typeAttribute, const String& consoleMessage) const;

    bool denyIfEnforcingPolicy() const { return m_reportOnly; }

    // FIXME: Make this a const reference once we teach applySandboxPolicy() to store its policy as opposed to applying it directly onto ContentSecurityPolicy.
    ContentSecurityPolicy& m_policy;

    String m_header;
    ContentSecurityPolicyHeaderType m_headerType;

    bool m_reportOnly;
    bool m_haveSandboxPolicy;
    ContentSecurityPolicy::ReflectedXSSDisposition m_reflectedXSSDisposition;

    std::unique_ptr<ContentSecurityPolicyMediaListDirective> m_pluginTypes;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_baseURI;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_connectSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_childSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_defaultSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_fontSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_formAction;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_frameAncestors;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_frameSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_imgSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_mediaSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_objectSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_scriptSrc;
    std::unique_ptr<ContentSecurityPolicySourceListDirective> m_styleSrc;
    
    Vector<String> m_reportURIs;
    
    String m_evalDisabledErrorMessage;
};

} // namespace WebCore

#endif /* ContentSecurityPolicyDirectiveList_h */
