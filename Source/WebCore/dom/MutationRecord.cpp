/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "MutationRecord.h"

#include "CharacterData.h"
#include "StaticNodeList.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

namespace {

class ChildListRecord : public MutationRecord {
public:
    ChildListRecord(ContainerNode& target, PassRefPtr<NodeList> added, PassRefPtr<NodeList> removed, PassRefPtr<Node> previousSibling, PassRefPtr<Node> nextSibling)
        : m_target(target)
        , m_addedNodes(added)
        , m_removedNodes(removed)
        , m_previousSibling(previousSibling)
        , m_nextSibling(nextSibling)
    {
    }

private:
    const AtomicString& type() override;
    Node* target() override { return m_target.ptr(); }
    NodeList* addedNodes() override { return m_addedNodes.get(); }
    NodeList* removedNodes() override { return m_removedNodes.get(); }
    Node* previousSibling() override { return m_previousSibling.get(); }
    Node* nextSibling() override { return m_nextSibling.get(); }

    Ref<ContainerNode> m_target;
    RefPtr<NodeList> m_addedNodes;
    RefPtr<NodeList> m_removedNodes;
    RefPtr<Node> m_previousSibling;
    RefPtr<Node> m_nextSibling;
};

class RecordWithEmptyNodeLists : public MutationRecord {
public:
    RecordWithEmptyNodeLists(Node& target, const String& oldValue)
        : m_target(target)
        , m_oldValue(oldValue)
    {
    }

private:
    Node* target() override { return m_target.ptr(); }
    String oldValue() override { return m_oldValue; }
    NodeList* addedNodes() override { return lazilyInitializeEmptyNodeList(m_addedNodes); }
    NodeList* removedNodes() override { return lazilyInitializeEmptyNodeList(m_removedNodes); }

    static NodeList* lazilyInitializeEmptyNodeList(RefPtr<NodeList>& nodeList)
    {
        if (!nodeList)
            nodeList = StaticNodeList::createEmpty();
        return nodeList.get();
    }

    Ref<Node> m_target;
    String m_oldValue;
    RefPtr<NodeList> m_addedNodes;
    RefPtr<NodeList> m_removedNodes;
};

class AttributesRecord : public RecordWithEmptyNodeLists {
public:
    AttributesRecord(Element& target, const QualifiedName& name, const AtomicString& oldValue)
        : RecordWithEmptyNodeLists(target, oldValue)
        , m_attributeName(name.localName())
        , m_attributeNamespace(name.namespaceURI())
    {
    }

private:
    const AtomicString& type() override;
    const AtomicString& attributeName() override { return m_attributeName; }
    const AtomicString& attributeNamespace() override { return m_attributeNamespace; }

    AtomicString m_attributeName;
    AtomicString m_attributeNamespace;
};

class CharacterDataRecord : public RecordWithEmptyNodeLists {
public:
    CharacterDataRecord(CharacterData& target, const String& oldValue)
        : RecordWithEmptyNodeLists(target, oldValue)
    {
    }

private:
    const AtomicString& type() override;
};

class MutationRecordWithNullOldValue : public MutationRecord {
public:
    MutationRecordWithNullOldValue(MutationRecord& record)
        : m_record(record)
    {
    }

private:
    const AtomicString& type() override { return m_record->type(); }
    Node* target() override { return m_record->target(); }
    NodeList* addedNodes() override { return m_record->addedNodes(); }
    NodeList* removedNodes() override { return m_record->removedNodes(); }
    Node* previousSibling() override { return m_record->previousSibling(); }
    Node* nextSibling() override { return m_record->nextSibling(); }
    const AtomicString& attributeName() override { return m_record->attributeName(); }
    const AtomicString& attributeNamespace() override { return m_record->attributeNamespace(); }

    String oldValue() override { return String(); }

    Ref<MutationRecord> m_record;
};

const AtomicString& ChildListRecord::type()
{
    static NeverDestroyed<AtomicString> childList("childList", AtomicString::ConstructFromLiteral);
    return childList;
}

const AtomicString& AttributesRecord::type()
{
    static NeverDestroyed<AtomicString> attributes("attributes", AtomicString::ConstructFromLiteral);
    return attributes;
}

const AtomicString& CharacterDataRecord::type()
{
    static NeverDestroyed<AtomicString> characterData("characterData", AtomicString::ConstructFromLiteral);
    return characterData;
}

} // namespace

Ref<MutationRecord> MutationRecord::createChildList(ContainerNode& target, PassRefPtr<NodeList> added, PassRefPtr<NodeList> removed, PassRefPtr<Node> previousSibling, PassRefPtr<Node> nextSibling)
{
    return adoptRef(static_cast<MutationRecord&>(*new ChildListRecord(target, added, removed, previousSibling, nextSibling)));
}

Ref<MutationRecord> MutationRecord::createAttributes(Element& target, const QualifiedName& name, const AtomicString& oldValue)
{
    return adoptRef(static_cast<MutationRecord&>(*new AttributesRecord(target, name, oldValue)));
}

Ref<MutationRecord> MutationRecord::createCharacterData(CharacterData& target, const String& oldValue)
{
    return adoptRef(static_cast<MutationRecord&>(*new CharacterDataRecord(target, oldValue)));
}

Ref<MutationRecord> MutationRecord::createWithNullOldValue(MutationRecord& record)
{
    return adoptRef(static_cast<MutationRecord&>(*new MutationRecordWithNullOldValue(record)));
}

MutationRecord::~MutationRecord()
{
}

} // namespace WebCore
