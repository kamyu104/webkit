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

WebInspector.HeapSnapshotNodeProxy = class HeapSnapshotNodeProxy
{
    constructor(snapshotObjectId, identifier, className, size, retainedSize, internal, gcRoot)
    {
        this._proxyObjectId = snapshotObjectId;

        this.id = identifier;
        this.className = className;
        this.size = size;
        this.retainedSize = retainedSize;
        this.internal = internal;
        this.gcRoot = gcRoot;
    }

    // Static

    static deserialize(objectId, serializedNode)
    {
        let {id, className, size, retainedSize, internal, gcRoot} = serializedNode;
        return new WebInspector.HeapSnapshotNodeProxy(objectId, id, className, size, retainedSize, internal, gcRoot);
    }

    // Proxied

    shortestGCRootPath(callback)
    {
        WebInspector.HeapSnapshotWorkerProxy.singleton().callMethod(this._proxyObjectId, "shortestGCRootPath", this.id, (serializedPath) => {
            let isNode = false;
            let path = serializedPath.map((component) => {
                isNode = !isNode;
                if (isNode)
                    return WebInspector.HeapSnapshotNodeProxy.deserialize(this._proxyObjectId, component);
                return WebInspector.HeapSnapshotEdgeProxy.deserialize(this._proxyObjectId, component);
            });

            for (let i = 1; i < path.length; i += 2) {
                console.assert(path[i] instanceof WebInspector.HeapSnapshotEdgeProxy);
                let edge = path[i];
                edge.from = path[i - 1];
                edge.to = path[i + 1];
            }

            callback(path);
        });
    }

    dominatedNodes(callback)
    {
        WebInspector.HeapSnapshotWorkerProxy.singleton().callMethod(this._proxyObjectId, "dominatedNodes", this.id, (serializedNodes) => {
            callback(serializedNodes.map(WebInspector.HeapSnapshotNodeProxy.deserialize.bind(null, this._proxyObjectId)));
        });
    }

    retainedNodes(callback)
    {
        WebInspector.HeapSnapshotWorkerProxy.singleton().callMethod(this._proxyObjectId, "retainedNodes", this.id, (serializedNodes) => {
            callback(serializedNodes.map(WebInspector.HeapSnapshotNodeProxy.deserialize.bind(null, this._proxyObjectId)));
        });
    }

    retainers(callback)
    {
        WebInspector.HeapSnapshotWorkerProxy.singleton().callMethod(this._proxyObjectId, "retainers", this.id, (serializedNodes) => {
            callback(serializedNodes.map(WebInspector.HeapSnapshotNodeProxy.deserialize.bind(null, this._proxyObjectId)));
        });
    }
};
