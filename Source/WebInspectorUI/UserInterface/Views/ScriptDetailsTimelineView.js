/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

WebInspector.ScriptDetailsTimelineView = class ScriptDetailsTimelineView extends WebInspector.TimelineView
{
    constructor(timeline, extraArguments)
    {
        super(timeline, extraArguments);

        console.assert(timeline.type === WebInspector.TimelineRecord.Type.Script);

        let columns = {name: {}, location: {}, callCount: {}, startTime: {}, totalTime: {}, selfTime: {}, averageTime: {}};

        columns.name.title = WebInspector.UIString("Name");
        columns.name.width = "10%";
        columns.name.icon = true;
        columns.name.disclosure = true;

        columns.location.title = WebInspector.UIString("Location");
        columns.location.icon = true;
        columns.location.width = "15%";

        let isSamplingProfiler = !!window.ScriptProfilerAgent;
        if (isSamplingProfiler)
            columns.callCount.title = WebInspector.UIString("Samples");
        else {
            // COMPATIBILITY(iOS 9): ScriptProfilerAgent did not exist yet, we had call counts, not samples.
            columns.callCount.title = WebInspector.UIString("Calls");
        }
        columns.callCount.width = "5%";
        columns.callCount.aligned = "right";

        columns.startTime.title = WebInspector.UIString("Start Time");
        columns.startTime.width = "10%";
        columns.startTime.aligned = "right";

        columns.totalTime.title = WebInspector.UIString("Total Time");
        columns.totalTime.width = "10%";
        columns.totalTime.aligned = "right";

        columns.selfTime.title = WebInspector.UIString("Self Time");
        columns.selfTime.width = "10%";
        columns.selfTime.aligned = "right";

        columns.averageTime.title = WebInspector.UIString("Average Time");
        columns.averageTime.width = "10%";
        columns.averageTime.aligned = "right";

        for (var column in columns)
            columns[column].sortable = true;

        this._dataGrid = new WebInspector.ScriptTimelineDataGrid(null, columns, this);
        this._dataGrid.addEventListener(WebInspector.TimelineDataGrid.Event.FiltersDidChange, this._dataGridFiltersDidChange, this);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
        this._dataGrid.sortColumnIdentifierSetting = new WebInspector.Setting("script-timeline-view-sort", "startTime");
        this._dataGrid.sortOrderSetting = new WebInspector.Setting("script-timeline-view-sort-order", WebInspector.DataGrid.SortOrder.Ascending);

        this.element.classList.add("script");
        this.addSubview(this._dataGrid);

        timeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._scriptTimelineRecordAdded, this);
        timeline.addEventListener(WebInspector.Timeline.Event.Refreshed, this._scriptTimelineRecordRefreshed, this);

        this._pendingRecords = [];
    }

    // Public

    shown()
    {
        super.shown();

        this._dataGrid.shown();
    }

    hidden()
    {
        this._dataGrid.hidden();

        super.hidden();
    }

    closed()
    {
        console.assert(this.representedObject instanceof WebInspector.Timeline);
        this.representedObject.removeEventListener(null, null, this);

        this._dataGrid.closed();
    }

    get selectionPathComponents()
    {
        var dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return null;

        var pathComponents = [];

        while (dataGridNode && !dataGridNode.root) {
            console.assert(dataGridNode instanceof WebInspector.TimelineDataGridNode);
            if (dataGridNode.hidden)
                return null;

            let representedObject = null;
            if (dataGridNode instanceof WebInspector.ProfileNodeDataGridNode)
                representedObject = dataGridNode.profileNode;

            let pathComponent = new WebInspector.TimelineDataGridNodePathComponent(dataGridNode);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this.dataGridNodePathComponentSelected, this);
            pathComponents.unshift(pathComponent);
            dataGridNode = dataGridNode.parent;
        }

        return pathComponents;
    }

    reset()
    {
        super.reset();

        this._dataGrid.reset();

        this._pendingRecords = [];
    }

    // Protected

    canShowContentViewForTreeElement(treeElement)
    {
        if (treeElement instanceof WebInspector.ProfileNodeTreeElement)
            return !!treeElement.profileNode.sourceCodeLocation;
        return super.canShowContentViewForTreeElement(treeElement);
    }

    showContentViewForTreeElement(treeElement)
    {
        if (treeElement instanceof WebInspector.ProfileNodeTreeElement) {
            if (treeElement.profileNode.sourceCodeLocation)
                WebInspector.showOriginalOrFormattedSourceCodeLocation(treeElement.profileNode.sourceCodeLocation);
            return;
        }

        super.showContentViewForTreeElement(treeElement);
    }

    dataGridNodePathComponentSelected(event)
    {
        let dataGridNode = event.data.pathComponent.timelineDataGridNode;
        console.assert(dataGridNode.dataGrid === this._dataGrid);

        dataGridNode.revealAndSelect();
    }

    layout()
    {
        if (this.startTime !== this._oldStartTime || this.endTime !== this._oldEndTime) {
            let dataGridNode = this._dataGrid.children[0];
            while (dataGridNode) {
                dataGridNode.updateRangeTimes(this.startTime, this.endTime);
                if (dataGridNode.revealed)
                    dataGridNode.refreshIfNeeded();
                dataGridNode = dataGridNode.traverseNextNode(false, null, true);
            }

            this._oldStartTime = this.startTime;
            this._oldEndTime = this.endTime;
        }

        this._processPendingRecords();
    }

    // Private

    _processPendingRecords()
    {
        if (WebInspector.timelineManager.scriptProfilerIsTracking())
            return;

        if (!this._pendingRecords.length)
            return;

        let zeroTime = this.zeroTime;
        let startTime = this.startTime;
        let endTime = this.endTime;

        for (let scriptTimelineRecord of this._pendingRecords) {
            let rootNodes = [];
            if (scriptTimelineRecord.profile) {
                // FIXME: Support using the bottom-up tree once it is implemented.
                rootNodes = scriptTimelineRecord.profile.topDownRootNodes;
            }

            let dataGridNode = new WebInspector.ScriptTimelineDataGridNode(scriptTimelineRecord, zeroTime);
            this._dataGrid.addRowInSortOrder(null, dataGridNode);

            for (let profileNode of rootNodes) {
                let profileNodeDataGridNode = new WebInspector.ProfileNodeDataGridNode(profileNode, zeroTime, startTime, endTime);
                this._dataGrid.addRowInSortOrder(null, profileNodeDataGridNode, dataGridNode);
            }
        }

        this._pendingRecords = [];
    }

    _scriptTimelineRecordAdded(event)
    {
        var scriptTimelineRecord = event.data.record;
        console.assert(scriptTimelineRecord instanceof WebInspector.ScriptTimelineRecord);

        this._pendingRecords.push(scriptTimelineRecord);

        this.needsLayout();
    }

    _scriptTimelineRecordRefreshed(event)
    {
        this.needsLayout();
    }

    _dataGridFiltersDidChange(event)
    {
        // FIXME: <https://webkit.org/b/154924> Web Inspector: hook up grid row filtering in the new Timelines UI
    }

    _dataGridNodeSelected(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }
};
