/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "PageConsoleAgent.h"

#include "CommandLineAPIHost.h"
#include "InspectorDOMAgent.h"
#include "Node.h"
#include "WebInjectedScriptManager.h"

using namespace Inspector;

namespace WebCore {

PageConsoleAgent::PageConsoleAgent(WebAgentContext& context, InspectorDOMAgent* domAgent)
    : WebConsoleAgent(context)
    , m_inspectorDOMAgent(domAgent)
{
}

void PageConsoleAgent::clearMessages(ErrorString& errorString)
{
    m_inspectorDOMAgent->releaseDanglingNodes();

    WebConsoleAgent::clearMessages(errorString);
}

class InspectableNode final : public CommandLineAPIHost::InspectableObject {
public:
    explicit InspectableNode(Node* node) : m_node(node) { }
    Deprecated::ScriptValue get(JSC::ExecState* state) override
    {
        return InspectorDOMAgent::nodeAsScriptValue(state, m_node.get());
    }
private:
    RefPtr<Node> m_node;
};

void PageConsoleAgent::addInspectedNode(ErrorString& errorString, int nodeId)
{
    Node* node = m_inspectorDOMAgent->nodeForId(nodeId);
    if (!node || node->isInShadowTree()) {
        errorString = ASCIILiteral("nodeId is not valid");
        return;
    }

    if (CommandLineAPIHost* commandLineAPIHost = static_cast<WebInjectedScriptManager&>(m_injectedScriptManager).commandLineAPIHost())
        commandLineAPIHost->addInspectedObject(std::make_unique<InspectableNode>(node));
}

} // namespace WebCore
