/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington.
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
#include "TimelineRecordFactory.h"

#include "Event.h"
#include "FloatQuad.h"
#include "JSMainThreadExecState.h"
#include <inspector/InspectorProtocolObjects.h>
#include <inspector/ScriptBreakpoint.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <profiler/Profile.h>

using namespace Inspector;

namespace WebCore {

Ref<InspectorObject> TimelineRecordFactory::createGenericRecord(double startTime, int maxCallStackDepth)
{
    Ref<InspectorObject> record = InspectorObject::create();
    record->setDouble(ASCIILiteral("startTime"), startTime);

    if (maxCallStackDepth) {
        RefPtr<ScriptCallStack> stackTrace = createScriptCallStack(JSMainThreadExecState::currentState(), maxCallStackDepth);
        if (stackTrace && stackTrace->size())
            record->setValue(ASCIILiteral("stackTrace"), stackTrace->buildInspectorArray());
    }
    return record;
}

Ref<InspectorObject> TimelineRecordFactory::createFunctionCallData(const String& scriptName, int scriptLine)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString(ASCIILiteral("scriptName"), scriptName);
    data->setInteger(ASCIILiteral("scriptLine"), scriptLine);
    return data;
}

Ref<InspectorObject> TimelineRecordFactory::createConsoleProfileData(const String& title)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString(ASCIILiteral("title"), title);
    return data;
}

Ref<InspectorObject> TimelineRecordFactory::createProbeSampleData(const ScriptBreakpointAction& action, unsigned sampleId)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger(ASCIILiteral("probeId"), action.identifier);
    data->setInteger(ASCIILiteral("sampleId"), sampleId);
    return data;
}

Ref<InspectorObject> TimelineRecordFactory::createEventDispatchData(const Event& event)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString(ASCIILiteral("type"), event.type().string());
    return data;
}

Ref<InspectorObject> TimelineRecordFactory::createGenericTimerData(int timerId)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger(ASCIILiteral("timerId"), timerId);
    return data;
}

Ref<InspectorObject> TimelineRecordFactory::createTimerInstallData(int timerId, std::chrono::milliseconds timeout, bool singleShot)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger(ASCIILiteral("timerId"), timerId);
    data->setInteger(ASCIILiteral("timeout"), (int)timeout.count());
    data->setBoolean(ASCIILiteral("singleShot"), singleShot);
    return data;
}

Ref<InspectorObject> TimelineRecordFactory::createEvaluateScriptData(const String& url, double lineNumber)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString(ASCIILiteral("url"), url);
    data->setInteger(ASCIILiteral("lineNumber"), lineNumber);
    return data;
}

Ref<InspectorObject> TimelineRecordFactory::createTimeStampData(const String& message)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString(ASCIILiteral("message"), message);
    return data;
}

Ref<InspectorObject> TimelineRecordFactory::createAnimationFrameData(int callbackId)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger(ASCIILiteral("id"), callbackId);
    return data;
}

static Ref<InspectorArray> createQuad(const FloatQuad& quad)
{
    Ref<InspectorArray> array = InspectorArray::create();
    array->pushDouble(quad.p1().x());
    array->pushDouble(quad.p1().y());
    array->pushDouble(quad.p2().x());
    array->pushDouble(quad.p2().y());
    array->pushDouble(quad.p3().x());
    array->pushDouble(quad.p3().y());
    array->pushDouble(quad.p4().x());
    array->pushDouble(quad.p4().y());
    return array;
}

Ref<InspectorObject> TimelineRecordFactory::createPaintData(const FloatQuad& quad)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setArray(ASCIILiteral("clip"), createQuad(quad));
    return data;
}

void TimelineRecordFactory::appendLayoutRoot(InspectorObject* data, const FloatQuad& quad)
{
    data->setArray(ASCIILiteral("root"), createQuad(quad));
}

static Ref<Protocol::Timeline::CPUProfileNodeAggregateCallInfo> buildAggregateCallInfoInspectorObject(const JSC::ProfileNode* node)
{
    double startTime = node->calls()[0].startTime();
    double endTime = node->calls().last().startTime() + node->calls().last().elapsedTime();

    double totalTime = 0;
    for (const JSC::ProfileNode::Call& call : node->calls())
        totalTime += call.elapsedTime();

    return Protocol::Timeline::CPUProfileNodeAggregateCallInfo::create()
        .setCallCount(node->calls().size())
        .setStartTime(startTime)
        .setEndTime(endTime)
        .setTotalTime(totalTime)
        .release();
}

static Ref<Protocol::Timeline::CPUProfileNode> buildInspectorObject(const JSC::ProfileNode* node)
{
    auto result = Protocol::Timeline::CPUProfileNode::create()
        .setId(node->id())
        .setCallInfo(buildAggregateCallInfoInspectorObject(node))
        .release();

    if (!node->functionName().isEmpty())
        result->setFunctionName(node->functionName());

    if (!node->url().isEmpty()) {
        result->setUrl(node->url());
        result->setLineNumber(node->lineNumber());
        result->setColumnNumber(node->columnNumber());
    }

    if (!node->children().isEmpty()) {
        auto children = Protocol::Array<Protocol::Timeline::CPUProfileNode>::create();
        for (RefPtr<JSC::ProfileNode> profileNode : node->children())
            children->addItem(buildInspectorObject(profileNode.get()));
        result->setChildren(WTFMove(children));
    }

    return result;
}

static Ref<Protocol::Timeline::CPUProfile> buildProfileInspectorObject(const JSC::Profile* profile)
{
    auto rootNodes = Protocol::Array<Protocol::Timeline::CPUProfileNode>::create();
    for (RefPtr<JSC::ProfileNode> profileNode : profile->rootNode()->children())
        rootNodes->addItem(buildInspectorObject(profileNode.get()));

    return Protocol::Timeline::CPUProfile::create()
        .setRootNodes(WTFMove(rootNodes))
        .release();
}

void TimelineRecordFactory::appendProfile(InspectorObject* data, RefPtr<JSC::Profile>&& profile)
{
    data->setValue(ASCIILiteral("profile"), buildProfileInspectorObject(profile.get()));
}

} // namespace WebCore
