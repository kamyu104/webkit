/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "DOMSelection.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "Node.h"
#include "Range.h"
#include "TextIterator.h"
#include "TreeScope.h"
#include "htmlediting.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

static Node* selectionShadowAncestor(Frame* frame)
{
    Node* node = frame->selection().selection().base().anchorNode();
    if (!node)
        return 0;

    if (!node->isInShadowTree())
        return 0;

    return frame->document()->ancestorInThisScope(node);
}

DOMSelection::DOMSelection(const TreeScope* treeScope)
    : DOMWindowProperty(treeScope->rootNode().document().frame())
    , m_treeScope(treeScope)
{
}

void DOMSelection::clearTreeScope()
{
    m_treeScope = nullptr;
}

const VisibleSelection& DOMSelection::visibleSelection() const
{
    ASSERT(m_frame);
    return m_frame->selection().selection();
}

static Position anchorPosition(const VisibleSelection& selection)
{
    Position anchor = selection.isBaseFirst() ? selection.start() : selection.end();
    return anchor.parentAnchoredEquivalent();
}

static Position focusPosition(const VisibleSelection& selection)
{
    Position focus = selection.isBaseFirst() ? selection.end() : selection.start();
    return focus.parentAnchoredEquivalent();
}

static Position basePosition(const VisibleSelection& selection)
{
    return selection.base().parentAnchoredEquivalent();
}

static Position extentPosition(const VisibleSelection& selection)
{
    return selection.extent().parentAnchoredEquivalent();
}

Node* DOMSelection::anchorNode() const
{
    if (!m_frame)
        return 0;

    return shadowAdjustedNode(anchorPosition(visibleSelection()));
}

int DOMSelection::anchorOffset() const
{
    if (!m_frame)
        return 0;

    return shadowAdjustedOffset(anchorPosition(visibleSelection()));
}

Node* DOMSelection::focusNode() const
{
    if (!m_frame)
        return 0;

    return shadowAdjustedNode(focusPosition(visibleSelection()));
}

int DOMSelection::focusOffset() const
{
    if (!m_frame)
        return 0;

    return shadowAdjustedOffset(focusPosition(visibleSelection()));
}

Node* DOMSelection::baseNode() const
{
    if (!m_frame)
        return 0;

    return shadowAdjustedNode(basePosition(visibleSelection()));
}

int DOMSelection::baseOffset() const
{
    if (!m_frame)
        return 0;

    return shadowAdjustedOffset(basePosition(visibleSelection()));
}

Node* DOMSelection::extentNode() const
{
    if (!m_frame)
        return 0;

    return shadowAdjustedNode(extentPosition(visibleSelection()));
}

int DOMSelection::extentOffset() const
{
    if (!m_frame)
        return 0;

    return shadowAdjustedOffset(extentPosition(visibleSelection()));
}

bool DOMSelection::isCollapsed() const
{
    if (!m_frame || selectionShadowAncestor(m_frame))
        return true;
    return !m_frame->selection().isRange();
}

String DOMSelection::type() const
{
    if (!m_frame)
        return String();

    FrameSelection& selection = m_frame->selection();

    // This is a WebKit DOM extension, incompatible with an IE extension
    // IE has this same attribute, but returns "none", "text" and "control"
    // http://msdn.microsoft.com/en-us/library/ms534692(VS.85).aspx
    if (selection.isNone())
        return "None";
    if (selection.isCaret())
        return "Caret";
    return "Range";
}

int DOMSelection::rangeCount() const
{
    if (!m_frame)
        return 0;
    return m_frame->selection().isNone() ? 0 : 1;
}

void DOMSelection::collapse(Node* node, int offset, ExceptionCode& ec)
{
    if (!m_frame)
        return;

    if (offset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!isValidForPosition(node))
        return;

    // FIXME: Eliminate legacy editing positions
    m_frame->selection().moveTo(createLegacyEditingPosition(node, offset), DOWNSTREAM);
}

void DOMSelection::collapseToEnd(ExceptionCode& ec)
{
    if (!m_frame)
        return;

    const VisibleSelection& selection = m_frame->selection().selection();

    if (selection.isNone()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    m_frame->selection().moveTo(selection.end(), DOWNSTREAM);
}

void DOMSelection::collapseToStart(ExceptionCode& ec)
{
    if (!m_frame)
        return;

    const VisibleSelection& selection = m_frame->selection().selection();

    if (selection.isNone()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    m_frame->selection().moveTo(selection.start(), DOWNSTREAM);
}

void DOMSelection::empty()
{
    if (!m_frame)
        return;
    m_frame->selection().clear();
}

void DOMSelection::setBaseAndExtent(Node* baseNode, int baseOffset, Node* extentNode, int extentOffset, ExceptionCode& ec)
{
    if (!m_frame)
        return;

    if (baseOffset < 0 || extentOffset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!isValidForPosition(baseNode) || !isValidForPosition(extentNode))
        return;

    // FIXME: Eliminate legacy editing positions
    m_frame->selection().moveTo(createLegacyEditingPosition(baseNode, baseOffset), createLegacyEditingPosition(extentNode, extentOffset), DOWNSTREAM);
}

void DOMSelection::setPosition(Node* node, int offset, ExceptionCode& ec)
{
    if (!m_frame)
        return;
    if (offset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!isValidForPosition(node))
        return;

    // FIXME: Eliminate legacy editing positions
    m_frame->selection().moveTo(createLegacyEditingPosition(node, offset), DOWNSTREAM);
}

void DOMSelection::modify(const String& alterString, const String& directionString, const String& granularityString)
{
    if (!m_frame)
        return;

    FrameSelection::EAlteration alter;
    if (equalLettersIgnoringASCIICase(alterString, "extend"))
        alter = FrameSelection::AlterationExtend;
    else if (equalLettersIgnoringASCIICase(alterString, "move"))
        alter = FrameSelection::AlterationMove;
    else
        return;

    SelectionDirection direction;
    if (equalLettersIgnoringASCIICase(directionString, "forward"))
        direction = DirectionForward;
    else if (equalLettersIgnoringASCIICase(directionString, "backward"))
        direction = DirectionBackward;
    else if (equalLettersIgnoringASCIICase(directionString, "left"))
        direction = DirectionLeft;
    else if (equalLettersIgnoringASCIICase(directionString, "right"))
        direction = DirectionRight;
    else
        return;

    TextGranularity granularity;
    if (equalLettersIgnoringASCIICase(granularityString, "character"))
        granularity = CharacterGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "word"))
        granularity = WordGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentence"))
        granularity = SentenceGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "line"))
        granularity = LineGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraph"))
        granularity = ParagraphGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "lineboundary"))
        granularity = LineBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentenceboundary"))
        granularity = SentenceBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraphboundary"))
        granularity = ParagraphBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "documentboundary"))
        granularity = DocumentBoundary;
    else
        return;

    m_frame->selection().modify(alter, direction, granularity);
}

void DOMSelection::extend(Node* node, int offset, ExceptionCode& ec)
{
    if (!m_frame)
        return;

    if (!node) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (offset < 0 || offset > (node->offsetInCharacters() ? caretMaxOffset(node) : static_cast<int>(node->countChildNodes()))) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!isValidForPosition(node))
        return;

    // FIXME: Eliminate legacy editing positions
    m_frame->selection().setExtent(createLegacyEditingPosition(node, offset), DOWNSTREAM);
}

PassRefPtr<Range> DOMSelection::getRangeAt(int index, ExceptionCode& ec)
{
    if (!m_frame)
        return 0;

    if (index < 0 || index >= rangeCount()) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }

    // If you're hitting this, you've added broken multi-range selection support
    ASSERT(rangeCount() == 1);

    if (Node* shadowAncestor = selectionShadowAncestor(m_frame)) {
        ContainerNode* container = shadowAncestor->parentNodeGuaranteedHostFree();
        unsigned offset = shadowAncestor->computeNodeIndex();
        return Range::create(shadowAncestor->document(), container, offset, container, offset);
    }

    return m_frame->selection().selection().firstRange();
}

void DOMSelection::removeAllRanges()
{
    if (!m_frame)
        return;
    m_frame->selection().clear();
}

void DOMSelection::addRange(Range* r)
{
    if (!m_frame)
        return;
    if (!r)
        return;

    FrameSelection& selection = m_frame->selection();

    if (selection.isNone()) {
        selection.moveTo(r);
        return;
    }

    RefPtr<Range> range = selection.selection().toNormalizedRange();
    if (r->compareBoundaryPoints(Range::START_TO_START, range.get(), IGNORE_EXCEPTION) == -1) {
        // We don't support discontiguous selection. We don't do anything if r and range don't intersect.
        if (r->compareBoundaryPoints(Range::START_TO_END, range.get(), IGNORE_EXCEPTION) > -1) {
            if (r->compareBoundaryPoints(Range::END_TO_END, range.get(), IGNORE_EXCEPTION) == -1) {
                // The original range and r intersect.
                selection.moveTo(r->startPosition(), range->endPosition(), DOWNSTREAM);
            } else {
                // r contains the original range.
                selection.moveTo(r);
            }
        }
    } else {
        // We don't support discontiguous selection. We don't do anything if r and range don't intersect.
        ExceptionCode ec = 0;
        if (r->compareBoundaryPoints(Range::END_TO_START, range.get(), ec) < 1 && !ec) {
            if (r->compareBoundaryPoints(Range::END_TO_END, range.get(), IGNORE_EXCEPTION) == -1) {
                // The original range contains r.
                selection.moveTo(range.get());
            } else {
                // The original range and r intersect.
                selection.moveTo(range->startPosition(), r->endPosition(), DOWNSTREAM);
            }
        }
    }
}

void DOMSelection::deleteFromDocument()
{
    if (!m_frame)
        return;

    FrameSelection& selection = m_frame->selection();

    if (selection.isNone())
        return;

    if (isCollapsed())
        selection.modify(FrameSelection::AlterationExtend, DirectionBackward, CharacterGranularity);

    RefPtr<Range> selectedRange = selection.selection().toNormalizedRange();
    if (!selectedRange)
        return;

    selectedRange->deleteContents(ASSERT_NO_EXCEPTION);

    setBaseAndExtent(&selectedRange->startContainer(), selectedRange->startOffset(), &selectedRange->startContainer(), selectedRange->startOffset(), ASSERT_NO_EXCEPTION);
}

bool DOMSelection::containsNode(Node* n, bool allowPartial) const
{
    if (!m_frame)
        return false;

    FrameSelection& selection = m_frame->selection();

    if (!n || m_frame->document() != &n->document() || selection.isNone())
        return false;

    RefPtr<Node> node = n;
    RefPtr<Range> selectedRange = selection.selection().toNormalizedRange();

    ContainerNode* parentNode = node->parentNode();
    if (!parentNode || !parentNode->inDocument())
        return false;
    unsigned nodeIndex = node->computeNodeIndex();

    ExceptionCode ec = 0;
    bool nodeFullySelected = Range::compareBoundaryPoints(parentNode, nodeIndex, &selectedRange->startContainer(), selectedRange->startOffset(), ec) >= 0 && !ec
        && Range::compareBoundaryPoints(parentNode, nodeIndex + 1, &selectedRange->endContainer(), selectedRange->endOffset(), ec) <= 0 && !ec;
    ASSERT(!ec);
    if (nodeFullySelected)
        return true;

    bool nodeFullyUnselected = (Range::compareBoundaryPoints(parentNode, nodeIndex, &selectedRange->endContainer(), selectedRange->endOffset(), ec) > 0 && !ec)
        || (Range::compareBoundaryPoints(parentNode, nodeIndex + 1, &selectedRange->startContainer(), selectedRange->startOffset(), ec) < 0 && !ec);
    ASSERT(!ec);
    if (nodeFullyUnselected)
        return false;

    return allowPartial || node->isTextNode();
}

void DOMSelection::selectAllChildren(Node* n, ExceptionCode& ec)
{
    if (!n)
        return;

    // This doesn't (and shouldn't) select text node characters.
    setBaseAndExtent(n, 0, n, n->countChildNodes(), ec);
}

String DOMSelection::toString()
{
    if (!m_frame)
        return String();

    return plainText(m_frame->selection().selection().toNormalizedRange().get());
}

Node* DOMSelection::shadowAdjustedNode(const Position& position) const
{
    if (position.isNull())
        return 0;

    Node* containerNode = position.containerNode();
    Node* adjustedNode = m_treeScope->ancestorInThisScope(containerNode);

    if (!adjustedNode)
        return 0;

    if (containerNode == adjustedNode)
        return containerNode;

    return adjustedNode->parentNodeGuaranteedHostFree();
}

int DOMSelection::shadowAdjustedOffset(const Position& position) const
{
    if (position.isNull())
        return 0;

    Node* containerNode = position.containerNode();
    Node* adjustedNode = m_treeScope->ancestorInThisScope(containerNode);

    if (!adjustedNode)
        return 0;

    if (containerNode == adjustedNode)
        return position.computeOffsetInContainerNode();

    return adjustedNode->computeNodeIndex();
}

bool DOMSelection::isValidForPosition(Node* node) const
{
    ASSERT(m_frame);
    if (!node)
        return true;
    return &node->document() == m_frame->document();
}

} // namespace WebCore
