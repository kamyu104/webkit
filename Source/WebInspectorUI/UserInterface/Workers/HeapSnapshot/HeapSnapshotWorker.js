/*
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

importScripts(...[
    "HeapSnapshot.js"
]);

HeapSnapshotWorker = class HeapSnapshotWorker
{
    constructor()
    {
        this._nextObjectId = 1;
        this._objects = new Map;

        self.addEventListener("message", this._handleMessage.bind(this));
    }

    // Actions

    createSnapshot(snapshotString)
    {
        let objectId = this._nextObjectId++;
        let snapshot = new HeapSnapshot(objectId, snapshotString);
        this._objects.set(objectId, snapshot);
        return {objectId, snapshot: snapshot.serialize()};
    }

    createSnapshotDiff(objectId1, objectId2)
    {
        let snapshot1 = this._objects.get(objectId1);
        let snapshot2 = this._objects.get(objectId2);

        console.assert(snapshot1 instanceof HeapSnapshot);
        console.assert(snapshot2 instanceof HeapSnapshot);

        let objectId = this._nextObjectId++;
        let snapshotDiff = new HeapSnapshotDiff(objectId, snapshot1, snapshot2);
        this._objects.set(objectId, snapshotDiff);
        return {objectId, snapshotDiff: snapshotDiff.serialize()};
    }

    // Public

    sendEvent(eventName, eventData)
    {
        self.postMessage({eventName, eventData});
    }

    // Private
    
    _handleMessage(event)
    {
        let data = event.data;

        // Action.
        if (data.actionName) {
            let result = this[data.actionName](...data.actionArguments);
            self.postMessage({callId: data.callId, result});
            return;
        }

        // Method.
        if (data.methodName) {
            console.assert(data.objectId, "Must have an objectId to call the method on");
            let object = this._objects.get(data.objectId);
            let result = object[data.methodName](...data.methodArguments);
            self.postMessage({callId: data.callId, result});
            return;
        }

        console.error("Unexpected HeapSnapshotWorker message", data);
    }
};

self.heapSnapshotWorker = new HeapSnapshotWorker;
