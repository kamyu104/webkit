/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 *
 */

#ifndef EventContext_h
#define EventContext_h

#include "EventTarget.h"
#include "Node.h"
#include "TreeScope.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class Event;
#if ENABLE(TOUCH_EVENTS)
class TouchList;
#endif

class EventContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // FIXME: Use ContainerNode instead of Node.
    EventContext(PassRefPtr<Node>, PassRefPtr<EventTarget> currentTarget, PassRefPtr<EventTarget> target);
    virtual ~EventContext();

    Node* node() const { return m_node.get(); }
    EventTarget* currentTarget() const { return m_currentTarget.get(); }
    EventTarget* target() const { return m_target.get(); }
    bool currentTargetSameAsTarget() const { return m_currentTarget.get() == m_target.get(); }
    virtual void handleLocalEvents(Event&) const;
    virtual bool isMouseOrFocusEventContext() const;
    virtual bool isTouchEventContext() const;

protected:
#if !ASSERT_DISABLED
    bool isUnreachableNode(EventTarget*);
    bool isReachable(Node*) const;
#endif
    RefPtr<Node> m_node;
    RefPtr<EventTarget> m_currentTarget;
    RefPtr<EventTarget> m_target;
};

class MouseOrFocusEventContext final : public EventContext {
public:
    MouseOrFocusEventContext(PassRefPtr<Node>, PassRefPtr<EventTarget> currentTarget, PassRefPtr<EventTarget> target);
    virtual ~MouseOrFocusEventContext();
    EventTarget* relatedTarget() const { return m_relatedTarget.get(); }
    void setRelatedTarget(PassRefPtr<EventTarget>);
    void handleLocalEvents(Event&) const override;
    bool isMouseOrFocusEventContext() const override;

private:
    RefPtr<EventTarget> m_relatedTarget;
};


#if ENABLE(TOUCH_EVENTS)
class TouchEventContext final : public EventContext {
public:
    TouchEventContext(PassRefPtr<Node>, PassRefPtr<EventTarget> currentTarget, PassRefPtr<EventTarget> target);
    virtual ~TouchEventContext();

    void handleLocalEvents(Event&) const override;
    bool isTouchEventContext() const override;

    enum TouchListType { Touches, TargetTouches, ChangedTouches, NotTouchList };
    TouchList* touchList(TouchListType type)
    {
        switch (type) {
        case Touches:
            return m_touches.get();
        case TargetTouches:
            return m_targetTouches.get();
        case ChangedTouches:
            return m_changedTouches.get();
        case NotTouchList:
            break;
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    TouchList* touches() { return m_touches.get(); }
    TouchList* targetTouches() { return m_targetTouches.get(); }
    TouchList* changedTouches() { return m_changedTouches.get(); }

private:
    RefPtr<TouchList> m_touches;
    RefPtr<TouchList> m_targetTouches;
    RefPtr<TouchList> m_changedTouches;
#if !ASSERT_DISABLED
    void checkReachability(TouchList*) const;
#endif
};

inline TouchEventContext& toTouchEventContext(EventContext& eventContext)
{
    ASSERT_WITH_SECURITY_IMPLICATION(eventContext.isTouchEventContext());
    return static_cast<TouchEventContext&>(eventContext);
}

inline TouchEventContext* toTouchEventContext(EventContext* eventContext)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!eventContext || eventContext->isTouchEventContext());
    return static_cast<TouchEventContext*>(eventContext);
}
#endif // ENABLE(TOUCH_EVENTS) && !PLATFORM(IOS)

#if !ASSERT_DISABLED
inline bool EventContext::isUnreachableNode(EventTarget* target)
{
    // FIXME: Checks also for SVG elements.
    return target && target->toNode() && !target->toNode()->isSVGElement() && !isReachable(target->toNode());
}

inline bool EventContext::isReachable(Node* target) const
{
    ASSERT(target);
    TreeScope& targetScope = target->treeScope();
    for (TreeScope* scope = &m_node->treeScope(); scope; scope = scope->parentTreeScope()) {
        if (scope == &targetScope)
            return true;
    }
    return false;
}
#endif

inline void MouseOrFocusEventContext::setRelatedTarget(PassRefPtr<EventTarget> relatedTarget)
{
    ASSERT(!isUnreachableNode(relatedTarget.get()));
    m_relatedTarget = relatedTarget;
}

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MouseOrFocusEventContext)
static bool isType(const WebCore::EventContext& context) { return context.isMouseOrFocusEventContext(); }
SPECIALIZE_TYPE_TRAITS_END()

#if ENABLE(TOUCH_EVENTS)
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TouchEventContext)
static bool isType(const WebCore::EventContext& context) { return context.isTouchEventContext(); }
SPECIALIZE_TYPE_TRAITS_END()
#endif

#endif // EventContext_h
