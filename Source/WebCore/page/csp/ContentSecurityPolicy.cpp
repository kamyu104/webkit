/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ContentSecurityPolicy.h"

#include "ContentSecurityPolicyDirective.h"
#include "ContentSecurityPolicyDirectiveList.h"
#include "ContentSecurityPolicyHash.h"
#include "ContentSecurityPolicySource.h"
#include "ContentSecurityPolicySourceList.h"
#include "CryptoDigest.h"
#include "DOMStringList.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FormData.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLParserIdioms.h"
#include "InspectorInstrumentation.h"
#include "JSDOMWindowShell.h"
#include "JSMainThreadExecState.h"
#include "ParsingUtilities.h"
#include "PingLoader.h"
#include "RuntimeEnabledFeatures.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "SecurityPolicyViolationEvent.h"
#include "TextEncoding.h"
#include <inspector/InspectorValues.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <wtf/TemporaryChange.h>
#include <wtf/text/TextPosition.h>

using namespace Inspector;

namespace WebCore {

ContentSecurityPolicy::ContentSecurityPolicy(ScriptExecutionContext& scriptExecutionContext)
    : m_scriptExecutionContext(&scriptExecutionContext)
    , m_sandboxFlags(SandboxNone)
{
    ASSERT(scriptExecutionContext.securityOrigin());
    auto& securityOrigin = *scriptExecutionContext.securityOrigin();
    m_selfSourceProtocol = securityOrigin.protocol();
    m_selfSource = std::make_unique<ContentSecurityPolicySource>(*this, m_selfSourceProtocol, securityOrigin.host(), securityOrigin.port(), emptyString(), false, false);
}

ContentSecurityPolicy::ContentSecurityPolicy(const SecurityOrigin& securityOrigin, const Frame* frame)
    : m_frame(frame)
    , m_sandboxFlags(SandboxNone)
{
    m_selfSourceProtocol = securityOrigin.protocol();
    m_selfSource = std::make_unique<ContentSecurityPolicySource>(*this, m_selfSourceProtocol, securityOrigin.host(), securityOrigin.port(), emptyString(), false, false);
}

ContentSecurityPolicy::~ContentSecurityPolicy()
{
}

void ContentSecurityPolicy::copyStateFrom(const ContentSecurityPolicy* other) 
{
    ASSERT(m_policies.isEmpty());
    for (auto& policy : other->m_policies)
        didReceiveHeader(policy->header(), policy->headerType(), ContentSecurityPolicy::PolicyFrom::Inherited);
}

void ContentSecurityPolicy::didCreateWindowShell(JSDOMWindowShell& windowShell) const
{
    JSDOMWindow* window = windowShell.window();
    ASSERT(window);
    ASSERT(window->scriptExecutionContext());
    ASSERT(window->scriptExecutionContext()->contentSecurityPolicy() == this);
    if (!windowShell.world().isNormal()) {
        window->setEvalEnabled(true);
        return;
    }
    window->setEvalEnabled(m_lastPolicyEvalDisabledErrorMessage.isNull(), m_lastPolicyEvalDisabledErrorMessage);
}

ContentSecurityPolicyResponseHeaders ContentSecurityPolicy::responseHeaders() const
{
    ContentSecurityPolicyResponseHeaders result;
    result.m_headers.reserveInitialCapacity(m_policies.size());
    for (auto& policy : m_policies)
        result.m_headers.uncheckedAppend({ policy->header(), policy->headerType() });
    return result;
}

void ContentSecurityPolicy::didReceiveHeaders(const ContentSecurityPolicyResponseHeaders& headers, ReportParsingErrors reportParsingErrors)
{
    TemporaryChange<bool> isReportingEnabled(m_isReportingEnabled, reportParsingErrors == ReportParsingErrors::Yes);
    for (auto& header : headers.m_headers)
        didReceiveHeader(header.first, header.second, ContentSecurityPolicy::PolicyFrom::HTTPHeader);
}

void ContentSecurityPolicy::didReceiveHeader(const String& header, ContentSecurityPolicyHeaderType type, ContentSecurityPolicy::PolicyFrom policyFrom)
{
    // RFC2616, section 4.2 specifies that headers appearing multiple times can
    // be combined with a comma. Walk the header string, and parse each comma
    // separated chunk as a separate header.
    auto characters = StringView(header).upconvertedCharacters();
    const UChar* begin = characters;
    const UChar* position = begin;
    const UChar* end = begin + header.length();
    while (position < end) {
        skipUntil<UChar>(position, end, ',');

        // header1,header2 OR header1
        //        ^                  ^
        std::unique_ptr<ContentSecurityPolicyDirectiveList> policy = ContentSecurityPolicyDirectiveList::create(*this, String(begin, position - begin), type, policyFrom);
        if (!policy->allowEval(nullptr, ContentSecurityPolicyDirectiveList::ReportingStatus::SuppressReport))
            m_lastPolicyEvalDisabledErrorMessage = policy->evalDisabledErrorMessage();

        m_policies.append(policy.release());

        // Skip the comma, and begin the next header from the current position.
        ASSERT(position == end || *position == ',');
        skipExactly<UChar>(position, end, ',');
        begin = position;
    }

    if (m_scriptExecutionContext)
        applyPolicyToScriptExecutionContext();
}

void ContentSecurityPolicy::applyPolicyToScriptExecutionContext()
{
    ASSERT(m_scriptExecutionContext);
    if (!m_lastPolicyEvalDisabledErrorMessage.isNull())
        m_scriptExecutionContext->disableEval(m_lastPolicyEvalDisabledErrorMessage);
    if (m_sandboxFlags != SandboxNone && is<Document>(m_scriptExecutionContext))
        m_scriptExecutionContext->enforceSandboxFlags(m_sandboxFlags);
}

void ContentSecurityPolicy::setOverrideAllowInlineStyle(bool value)
{
    m_overrideInlineStyleAllowed = value;
}

bool ContentSecurityPolicy::urlMatchesSelf(const URL& url) const
{
    return m_selfSource->matches(url);
}

bool ContentSecurityPolicy::protocolMatchesSelf(const URL& url) const
{
    if (equalLettersIgnoringASCIICase(m_selfSourceProtocol, "http"))
        return url.protocolIsInHTTPFamily();
    return equalIgnoringASCIICase(url.protocol(), m_selfSourceProtocol);
}

template<bool (ContentSecurityPolicyDirectiveList::*allowed)(const Frame&, const URL&, ContentSecurityPolicyDirectiveList::ReportingStatus) const>
static bool isAllowedByAllWithFrame(const CSPDirectiveListVector& policies, const Frame& frame, const URL& url, ContentSecurityPolicyDirectiveList::ReportingStatus reportingStatus)
{
    for (auto& policy : policies) {
        if (!(policy.get()->*allowed)(frame, url, reportingStatus))
            return false;
    }
    return true;
}

template<bool (ContentSecurityPolicyDirectiveList::*allowed)(ContentSecurityPolicyDirectiveList::ReportingStatus) const>
static bool isAllowedByAll(const CSPDirectiveListVector& policies, ContentSecurityPolicyDirectiveList::ReportingStatus reportingStatus)
{
    for (auto& policy : policies) {
        if (!(policy.get()->*allowed)(reportingStatus))
            return false;
    }
    return true;
}

template<bool (ContentSecurityPolicyDirectiveList::*allowed)(JSC::ExecState* state, ContentSecurityPolicyDirectiveList::ReportingStatus) const>
static bool isAllowedByAllWithState(const CSPDirectiveListVector& policies, JSC::ExecState* state, ContentSecurityPolicyDirectiveList::ReportingStatus reportingStatus)
{
    for (auto& policy : policies) {
        if (!(policy.get()->*allowed)(state, reportingStatus))
            return false;
    }
    return true;
}

template<bool (ContentSecurityPolicyDirectiveList::*allowed)(const String&, const WTF::OrdinalNumber&, ContentSecurityPolicyDirectiveList::ReportingStatus) const>
static bool isAllowedByAllWithContext(const CSPDirectiveListVector& policies, const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicyDirectiveList::ReportingStatus reportingStatus)
{
    for (auto& policy : policies) {
        if (!(policy.get()->*allowed)(contextURL, contextLine, reportingStatus))
            return false;
    }
    return true;
}

template<bool (ContentSecurityPolicyDirectiveList::*allowed)(const String& nonce) const>
static bool isAllowedByAllWithNonce(const CSPDirectiveListVector& policies, const String& nonce)
{
    for (auto& policy : policies) {
        if (!(policy.get()->*allowed)(nonce))
            return false;
    }
    return true;
}

static CryptoDigest::Algorithm toCryptoDigestAlgorithm(ContentSecurityPolicyHashAlgorithm algorithm)
{
    switch (algorithm) {
    case ContentSecurityPolicyHashAlgorithm::SHA_256:
        return CryptoDigest::Algorithm::SHA_256;
    case ContentSecurityPolicyHashAlgorithm::SHA_384:
        return CryptoDigest::Algorithm::SHA_384;
    case ContentSecurityPolicyHashAlgorithm::SHA_512:
        return CryptoDigest::Algorithm::SHA_512;
    }
    ASSERT_NOT_REACHED();
    return CryptoDigest::Algorithm::SHA_512;
}

template<bool (ContentSecurityPolicyDirectiveList::*allowed)(const ContentSecurityPolicyHash&) const>
static bool isAllowedByAllWithHashFromContent(const CSPDirectiveListVector& policies, const String& content, const TextEncoding& encoding, OptionSet<ContentSecurityPolicyHashAlgorithm> algorithms)
{
    // FIXME: Compute the digest with respect to the raw bytes received from the page.
    // See <https://bugs.webkit.org/show_bug.cgi?id=155184>.
    CString contentCString = encoding.encode(content, EntitiesForUnencodables);
    for (auto algorithm : algorithms) {
        auto cryptoDigest = CryptoDigest::create(toCryptoDigestAlgorithm(algorithm));
        cryptoDigest->addBytes(contentCString.data(), contentCString.length());
        Vector<uint8_t> digest = cryptoDigest->computeHash();
        for (auto& policy : policies) {
            if ((policy.get()->*allowed)(std::make_pair(algorithm, digest)))
                return true;
        }
    }
    return false;
}

template<bool (ContentSecurityPolicyDirectiveList::*allowFromURL)(const URL&, ContentSecurityPolicyDirectiveList::ReportingStatus) const>
static bool isAllowedByAllWithURL(const CSPDirectiveListVector& policies, const URL& url, ContentSecurityPolicyDirectiveList::ReportingStatus reportingStatus)
{
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;

    for (auto& policy : policies) {
        if (!(policy.get()->*allowFromURL)(url, reportingStatus))
            return false;
    }
    return true;
}

bool ContentSecurityPolicy::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithContext<&ContentSecurityPolicyDirectiveList::allowJavaScriptURLs>(m_policies, contextURL, contextLine, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithContext<&ContentSecurityPolicyDirectiveList::allowInlineEventHandlers>(m_policies, contextURL, contextLine, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

// FIXME: We should compute the document encoding once and cache it instead of computing it on each invocation.
const TextEncoding& ContentSecurityPolicy::documentEncoding() const
{
    if (!is<Document>(m_scriptExecutionContext))
        return UTF8Encoding();
    Document& document = downcast<Document>(*m_scriptExecutionContext);
    if (TextResourceDecoder* decoder = document.decoder())
        return decoder->encoding();
    return UTF8Encoding();
}

bool ContentSecurityPolicy::allowScriptWithNonce(const String& nonce, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    if (strippedNonce.isEmpty())
        return false;
    if (isAllowedByAllWithNonce<&ContentSecurityPolicyDirectiveList::allowScriptWithNonce>(m_policies, strippedNonce))
        return true;
    return false;
}

bool ContentSecurityPolicy::allowStyleWithNonce(const String& nonce, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    String strippedNonce = stripLeadingAndTrailingHTMLSpaces(nonce);
    if (strippedNonce.isEmpty())
        return false;
    if (isAllowedByAllWithNonce<&ContentSecurityPolicyDirectiveList::allowStyleWithNonce>(m_policies, strippedNonce))
        return true;
    return false;
}

bool ContentSecurityPolicy::allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& scriptContent, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (!m_hashAlgorithmsForInlineScripts.isEmpty() && !scriptContent.isEmpty()
        && isAllowedByAllWithHashFromContent<&ContentSecurityPolicyDirectiveList::allowInlineScriptWithHash>(m_policies, scriptContent, documentEncoding(), m_hashAlgorithmsForInlineScripts))
        return true;
    return isAllowedByAllWithContext<&ContentSecurityPolicyDirectiveList::allowInlineScript>(m_policies, contextURL, contextLine, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, const String& styleContent, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    if (m_overrideInlineStyleAllowed)
        return true;
    if (!m_hashAlgorithmsForInlineStylesheets.isEmpty() && !styleContent.isEmpty()
        && isAllowedByAllWithHashFromContent<&ContentSecurityPolicyDirectiveList::allowInlineStyleWithHash>(m_policies, styleContent, documentEncoding(), m_hashAlgorithmsForInlineStylesheets))
        return true;
    return isAllowedByAllWithContext<&ContentSecurityPolicyDirectiveList::allowInlineStyle>(m_policies, contextURL, contextLine, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowEval(JSC::ExecState* state, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithState<&ContentSecurityPolicyDirectiveList::allowEval>(m_policies, state, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowFrameAncestors(const Frame& frame, const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    Frame& topFrame = frame.tree().top();
    if (&frame == &topFrame)
        return true;
    return isAllowedByAllWithFrame<&ContentSecurityPolicyDirectiveList::allowFrameAncestors>(m_policies, frame, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowPluginType(const String& type, const String& typeAttribute, const URL& url, bool overrideContentSecurityPolicy) const
{
    if (overrideContentSecurityPolicy)
        return true;
    for (auto& policy : m_policies) {
        if (!policy->allowPluginType(type, typeAttribute, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport))
            return false;
    }
    return true;
}

bool ContentSecurityPolicy::allowScriptFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowScriptFromSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowObjectFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowObjectFromSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowChildFrameFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowChildFrameFromSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowChildContextFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowChildContextFromSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowImageFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowImageFromSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowStyleFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowStyleFromSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowFontFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowFontFromSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowMediaFromSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowMediaFromSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowConnectToSource(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowConnectToSource>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowFormAction(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowFormAction>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::allowBaseURI(const URL& url, bool overrideContentSecurityPolicy) const
{
    return overrideContentSecurityPolicy || isAllowedByAllWithURL<&ContentSecurityPolicyDirectiveList::allowBaseURI>(m_policies, url, ContentSecurityPolicyDirectiveList::ReportingStatus::SendReport);
}

bool ContentSecurityPolicy::isActive() const
{
    return !m_policies.isEmpty();
}

ContentSecurityPolicy::ReflectedXSSDisposition ContentSecurityPolicy::reflectedXSSDisposition() const
{
    ReflectedXSSDisposition disposition = ReflectedXSSUnset;
    for (auto& policy : m_policies) {
        if (policy->reflectedXSSDisposition() > disposition)
            disposition = std::max(disposition, policy->reflectedXSSDisposition());
    }
    return disposition;
}

void ContentSecurityPolicy::gatherReportURIs(DOMStringList& list) const
{
    ASSERT(m_scriptExecutionContext);
    for (auto& policy : m_policies) {
        for (auto& url : policy->reportURIs())
            list.append(m_scriptExecutionContext->completeURL(url).string());
    }
}

static String stripURLForUseInReport(Document& document, const URL& url)
{
    if (!url.isValid())
        return String();
    if (!url.isHierarchical() || url.protocolIs("file"))
        return url.protocol();
    return document.securityOrigin()->canRequest(url) ? url.strippedForUseAsReferrer() : SecurityOrigin::create(url).get().toString();
}

void ContentSecurityPolicy::reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const URL& blockedURL, const Vector<String>& reportURIs, const String& header, const String& contextURL, const WTF::OrdinalNumber& contextLine, JSC::ExecState* state) const
{
    logToConsole(consoleMessage, contextURL, contextLine, state);

    if (!m_isReportingEnabled)
        return;

    // FIXME: Support sending reports from worker.
    if (!is<Document>(m_scriptExecutionContext) && !m_frame)
        return;

    // FIXME: We should not hardcode the directive names. We should make use of the constants in ContentSecurityPolicyDirectiveList.cpp.
    // See <https://bugs.webkit.org/show_bug.cgi?id=155133>.
    ASSERT(!m_frame || effectiveDirective == "frame-ancestors");

    Document& document = is<Document>(m_scriptExecutionContext) ? downcast<Document>(*m_scriptExecutionContext) : *m_frame->document();
    Frame* frame = document.frame();
    ASSERT(!m_frame || m_frame == frame);
    if (!frame)
        return;

    String documentURI;
    String blockedURI;
    if (is<Document>(m_scriptExecutionContext)) {
        documentURI = document.url().strippedForUseAsReferrer();
        blockedURI = stripURLForUseInReport(document, blockedURL);
    } else {
        // The URL of |document| may not have been initialized (say, when reporting a frame-ancestors violation).
        // So, we use the URL of the blocked document for the protected document URL.
        documentURI = blockedURL;
        blockedURI = blockedURL;
    }
    String violatedDirective = directiveText;
    String originalPolicy = header;
    String referrer = document.referrer();
    ASSERT(document.loader());
    unsigned short statusCode = document.url().protocolIs("http") && document.loader() ? document.loader()->response().httpStatusCode() : 0;

    String sourceFile;
    int lineNumber = 0;
    int columnNumber = 0;
    RefPtr<ScriptCallStack> stack = createScriptCallStack(JSMainThreadExecState::currentState(), 2);
    const ScriptCallFrame* callFrame = stack->firstNonNativeCallFrame();
    if (callFrame && callFrame->lineNumber()) {
        sourceFile = stripURLForUseInReport(document, URL(URL(), callFrame->sourceURL()));
        lineNumber = callFrame->lineNumber();
        columnNumber = callFrame->columnNumber();
    }

    // 1. Dispatch violation event.
    bool canBubble = false;
    bool cancelable = false;
    document.enqueueDocumentEvent(SecurityPolicyViolationEvent::create(eventNames().securitypolicyviolationEvent, canBubble, cancelable, documentURI, referrer, blockedURI, violatedDirective, effectiveDirective, originalPolicy, sourceFile, statusCode, lineNumber, columnNumber));

    // 2. Send violation report (if applicable).
    if (reportURIs.isEmpty())
        return;

    // We need to be careful here when deciding what information to send to the
    // report-uri. Currently, we send only the current document's URL and the
    // directive that was violated. The document's URL is safe to send because
    // it's the document itself that's requesting that it be sent. You could
    // make an argument that we shouldn't send HTTPS document URLs to HTTP
    // report-uris (for the same reasons that we suppress the Referer in that
    // case), but the Referer is sent implicitly whereas this request is only
    // sent explicitly. As for which directive was violated, that's pretty
    // harmless information.

    RefPtr<InspectorObject> cspReport = InspectorObject::create();
    cspReport->setString(ASCIILiteral("document-uri"), documentURI);
    cspReport->setString(ASCIILiteral("referrer"), referrer);
    cspReport->setString(ASCIILiteral("violated-directive"), directiveText);
    cspReport->setString(ASCIILiteral("effective-directive"), effectiveDirective);
    cspReport->setString(ASCIILiteral("original-policy"), originalPolicy);
    cspReport->setString(ASCIILiteral("blocked-uri"), blockedURI);
    cspReport->setInteger(ASCIILiteral("status-code"), statusCode);
    if (!sourceFile.isNull()) {
        cspReport->setString(ASCIILiteral("source-file"), sourceFile);
        cspReport->setInteger(ASCIILiteral("line-number"), lineNumber);
        cspReport->setInteger(ASCIILiteral("column-number"), columnNumber);
    }

    RefPtr<InspectorObject> reportObject = InspectorObject::create();
    reportObject->setObject(ASCIILiteral("csp-report"), cspReport.release());

    RefPtr<FormData> report = FormData::create(reportObject->toJSONString().utf8());
    for (const auto& url : reportURIs)
        PingLoader::sendViolationReport(*frame, document.completeURL(url), report.copyRef(), ViolationReportType::ContentSecurityPolicy);
}

void ContentSecurityPolicy::reportUnsupportedDirective(const String& name) const
{
    String message;
    if (equalLettersIgnoringASCIICase(name, "allow"))
        message = ASCIILiteral("The 'allow' directive has been replaced with 'default-src'. Please use that directive instead, as 'allow' has no effect.");
    else if (equalLettersIgnoringASCIICase(name, "options"))
        message = ASCIILiteral("The 'options' directive has been replaced with 'unsafe-inline' and 'unsafe-eval' source expressions for the 'script-src' and 'style-src' directives. Please use those directives instead, as 'options' has no effect.");
    else if (equalLettersIgnoringASCIICase(name, "policy-uri"))
        message = ASCIILiteral("The 'policy-uri' directive has been removed from the specification. Please specify a complete policy via the Content-Security-Policy header.");
    else
        message = makeString("Unrecognized Content-Security-Policy directive '", name, "'.\n"); // FIXME: Why does this include a newline?

    logToConsole(message);
}

void ContentSecurityPolicy::reportDirectiveAsSourceExpression(const String& directiveName, const String& sourceExpression) const
{
    logToConsole("The Content Security Policy directive '" + directiveName + "' contains '" + sourceExpression + "' as a source expression. Did you mean '" + directiveName + " ...; " + sourceExpression + "...' (note the semicolon)?");
}

void ContentSecurityPolicy::reportDuplicateDirective(const String& name) const
{
    logToConsole(makeString("Ignoring duplicate Content-Security-Policy directive '", name, "'.\n"));
}

void ContentSecurityPolicy::reportInvalidPluginTypes(const String& pluginType) const
{
    String message;
    if (pluginType.isNull())
        message = "'plugin-types' Content Security Policy directive is empty; all plugins will be blocked.\n";
    else
        message = makeString("Invalid plugin type in 'plugin-types' Content Security Policy directive: '", pluginType, "'.\n");
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSandboxFlags(const String& invalidFlags) const
{
    logToConsole("Error while parsing the 'sandbox' Content Security Policy directive: " + invalidFlags);
}

void ContentSecurityPolicy::reportInvalidReflectedXSS(const String& invalidValue) const
{
    logToConsole("The 'reflected-xss' Content Security Policy directive has the invalid value \"" + invalidValue + "\". Value values are \"allow\", \"filter\", and \"block\".");
}

void ContentSecurityPolicy::reportInvalidDirectiveInReportOnlyMode(const String& directiveName) const
{
    logToConsole("The Content Security Policy directive '" + directiveName + "' is ignored when delivered in a report-only policy.");
}

void ContentSecurityPolicy::reportInvalidDirectiveInHTTPEquivMeta(const String& directiveName) const
{
    logToConsole("The Content Security Policy directive '" + directiveName + "' is ignored when delivered via an HTML meta element.");
}

void ContentSecurityPolicy::reportInvalidDirectiveValueCharacter(const String& directiveName, const String& value) const
{
    String message = makeString("The value for Content Security Policy directive '", directiveName, "' contains an invalid character: '", value, "'. Non-whitespace characters outside ASCII 0x21-0x7E must be percent-encoded, as described in RFC 3986, section 2.1: http://tools.ietf.org/html/rfc3986#section-2.1.");
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidPathCharacter(const String& directiveName, const String& value, const char invalidChar) const
{
    ASSERT(invalidChar == '#' || invalidChar == '?');

    String ignoring;
    if (invalidChar == '?')
        ignoring = "The query component, including the '?', will be ignored.";
    else
        ignoring = "The fragment identifier, including the '#', will be ignored.";

    String message = makeString("The source list for Content Security Policy directive '", directiveName, "' contains a source with an invalid path: '", value, "'. ", ignoring);
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSourceExpression(const String& directiveName, const String& source) const
{
    String message = makeString("The source list for Content Security Policy directive '", directiveName, "' contains an invalid source: '", source, "'. It will be ignored.");
    if (equalLettersIgnoringASCIICase(source, "'none'"))
        message = makeString(message, " Note that 'none' has no effect unless it is the only expression in the source list.");
    logToConsole(message);
}

void ContentSecurityPolicy::reportMissingReportURI(const String& policy) const
{
    logToConsole("The Content Security Policy '" + policy + "' was delivered in report-only mode, but does not specify a 'report-uri'; the policy will have no effect. Please either add a 'report-uri' directive, or deliver the policy via the 'Content-Security-Policy' header.");
}

void ContentSecurityPolicy::logToConsole(const String& message, const String& contextURL, const WTF::OrdinalNumber& contextLine, JSC::ExecState* state) const
{
    if (!m_isReportingEnabled)
        return;

    // FIXME: <http://webkit.org/b/114317> ContentSecurityPolicy::logToConsole should include a column number
    if (m_scriptExecutionContext)
        m_scriptExecutionContext->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, contextURL, contextLine.oneBasedInt(), 0, state);
    else if (m_frame && m_frame->document())
        static_cast<ScriptExecutionContext*>(m_frame->document())->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, contextURL, contextLine.oneBasedInt(), 0, state);
}

void ContentSecurityPolicy::reportBlockedScriptExecutionToInspector(const String& directiveText) const
{
    if (m_scriptExecutionContext)
        InspectorInstrumentation::scriptExecutionBlockedByCSP(m_scriptExecutionContext, directiveText);
}

bool ContentSecurityPolicy::experimentalFeaturesEnabled() const
{
#if ENABLE(CSP_NEXT)
    return RuntimeEnabledFeatures::sharedFeatures().experimentalContentSecurityPolicyFeaturesEnabled();
#else
    return false;
#endif
}
    
}
