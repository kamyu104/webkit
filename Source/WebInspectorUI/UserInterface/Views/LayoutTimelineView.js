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

WebInspector.LayoutTimelineView = class LayoutTimelineView extends WebInspector.TimelineView
{
    constructor(timeline, extraArguments)
    {
        super(timeline, extraArguments);

        console.assert(timeline.type === WebInspector.TimelineRecord.Type.Layout, timeline);

        let columns = {name: {}, location: {}, width: {}, height: {}, startTime: {}, totalTime: {}};

        columns.name.title = WebInspector.UIString("Type");
        columns.name.width = "15%";

        var typeToLabelMap = new Map;
        for (var key in WebInspector.LayoutTimelineRecord.EventType) {
            var value = WebInspector.LayoutTimelineRecord.EventType[key];
            typeToLabelMap.set(value, WebInspector.LayoutTimelineRecord.displayNameForEventType(value));
        }

        columns.name.scopeBar = WebInspector.TimelineDataGrid.createColumnScopeBar("layout", typeToLabelMap);
        columns.name.disclosure = true;
        columns.name.icon = true;

        this._scopeBar = columns.name.scopeBar;

        columns.location.title = WebInspector.UIString("Initiator");
        columns.location.width = "25%";

        columns.width.title = WebInspector.UIString("Width");
        columns.width.width = "8%";

        columns.height.title = WebInspector.UIString("Height");
        columns.height.width = "8%";

        columns.startTime.title = WebInspector.UIString("Start Time");
        columns.startTime.width = "8%";
        columns.startTime.aligned = "right";

        columns.totalTime.title = WebInspector.UIString("Duration");
        columns.totalTime.width = "8%";
        columns.totalTime.aligned = "right";

        for (var column in columns)
            columns[column].sortable = true;

        this._dataGrid = new WebInspector.LayoutTimelineDataGrid(null, columns);
        this._dataGrid.addEventListener(WebInspector.TimelineDataGrid.Event.FiltersDidChange, this._dataGridFiltersDidChange, this);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);

        this._dataGrid.sortColumnIdentifierSetting = new WebInspector.Setting("layout-timeline-view-sort", "startTime");
        this._dataGrid.sortOrderSetting = new WebInspector.Setting("layout-timeline-view-sort-order", WebInspector.DataGrid.SortOrder.Ascending);

        this._hoveredTreeElement = null;
        this._hoveredDataGridNode = null;
        this._showingHighlight = false;
        this._showingHighlightForRecord = null;

        this._dataGrid.element.addEventListener("mouseover", this._mouseOverDataGrid.bind(this));
        this._dataGrid.element.addEventListener("mouseleave", this._mouseLeaveDataGrid.bind(this));

        this.element.classList.add("layout");
        this.addSubview(this._dataGrid);

        timeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._layoutTimelineRecordAdded, this);

        this._pendingRecords = [];
    }

    // Public

    get selectionPathComponents()
    {
        let dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode || dataGridNode.hidden)
            return null;

        let pathComponents = [];

        while (dataGridNode && !dataGridNode.root) {
            console.assert(dataGridNode instanceof WebInspector.TimelineDataGridNode);
            if (dataGridNode.hidden)
                return null;

            let pathComponent = new WebInspector.TimelineDataGridNodePathComponent(dataGridNode);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this.dataGridNodePathComponentSelected, this);
            pathComponents.unshift(pathComponent);
            dataGridNode = dataGridNode.parent;
        }

        return pathComponents;
    }

    shown()
    {
        super.shown();

        this._updateHighlight();

        this._dataGrid.shown();
    }

    hidden()
    {
        this._hideHighlightIfNeeded();

        this._dataGrid.hidden();

        super.hidden();
    }

    closed()
    {
        console.assert(this.representedObject instanceof WebInspector.Timeline);
        this.representedObject.removeEventListener(null, null, this);

        this._dataGrid.closed();
    }

    filterDidChange()
    {
        super.filterDidChange();

        this._updateHighlight();
    }

    matchTreeElementAgainstCustomFilters(treeElement)
    {
        return this._dataGrid.treeElementMatchesActiveScopeFilters(treeElement);
    }

    reset()
    {
        super.reset();

        this._hideHighlightIfNeeded();

        this._dataGrid.reset();

        this._pendingRecords = [];
    }

    // Protected

    dataGridNodePathComponentSelected(event)
    {
        let dataGridNode = event.data.pathComponent.timelineDataGridNode;
        console.assert(dataGridNode.dataGrid === this._dataGrid);

        dataGridNode.revealAndSelect();
    }

    treeElementDeselected(treeElement)
    {
        super.treeElementDeselected(treeElement);

        this._updateHighlight();
    }

    treeElementSelected(treeElement, selectedByUser)
    {
        if (this._dataGrid.shouldIgnoreSelectionEvent())
            return;

        super.treeElementSelected(treeElement, selectedByUser);

        this._updateHighlight();
    }

    layout()
    {
        this._processPendingRecords();
    }

    // Private

    _processPendingRecords()
    {
        if (!this._pendingRecords.length)
            return;

        for (var layoutTimelineRecord of this._pendingRecords) {
            let dataGridNode = new WebInspector.LayoutTimelineDataGridNode(layoutTimelineRecord, this.zeroTime);

            this._dataGrid.addRowInSortOrder(null, dataGridNode);

            let stack = [{children: layoutTimelineRecord.children, parentDataGridNode: dataGridNode, index: 0}];
            while (stack.length) {
                let entry = stack.lastValue;
                if (entry.index >= entry.children.length) {
                    stack.pop();
                    continue;
                }

                let childRecord = entry.children[entry.index];
                console.assert(childRecord.type === WebInspector.TimelineRecord.Type.Layout, childRecord);

                let childDataGridNode = new WebInspector.LayoutTimelineDataGridNode(childRecord, this.zeroTime);
                console.assert(entry.parentDataGridNode, "entry without parent!");
                this._dataGrid.addRowInSortOrder(null, childDataGridNode, entry.parentDataGridNode);

                if (childDataGridNode && childRecord.children.length)
                    stack.push({children: childRecord.children, parentDataGridNode: childDataGridNode, index: 0});
                ++entry.index;
            }
        }

        this._pendingRecords = [];
    }

    _layoutTimelineRecordAdded(event)
    {
        var layoutTimelineRecord = event.data.record;
        console.assert(layoutTimelineRecord instanceof WebInspector.LayoutTimelineRecord);

        // Only add top-level records, to avoid processing child records multiple times.
        if (layoutTimelineRecord.parent instanceof WebInspector.LayoutTimelineRecord)
            return;

        this._pendingRecords.push(layoutTimelineRecord);

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

    _updateHighlight()
    {
        var record = this._hoveredOrSelectedRecord();
        if (!record) {
            this._hideHighlightIfNeeded();
            return;
        }

        this._showHighlightForRecord(record);
    }

    _showHighlightForRecord(record)
    {
        if (this._showingHighlightForRecord === record)
            return;

        this._showingHighlightForRecord = record;

        const contentColor = {r: 111, g: 168, b: 220, a: 0.66};
        const outlineColor = {r: 255, g: 229, b: 153, a: 0.66};

        var quad = record.quad;
        if (quad) {
            DOMAgent.highlightQuad(quad.toProtocol(), contentColor, outlineColor);
            this._showingHighlight = true;
            return;
        }

        // This record doesn't have a highlight, so hide any existing highlight.
        if (this._showingHighlight) {
            this._showingHighlight = false;
            DOMAgent.hideHighlight();
        }
    }

    _hideHighlightIfNeeded()
    {
        this._showingHighlightForRecord = null;

        if (this._showingHighlight) {
            this._showingHighlight = false;
            DOMAgent.hideHighlight();
        }
    }

    _hoveredOrSelectedRecord()
    {
        if (this._hoveredDataGridNode)
            return this._hoveredDataGridNode.record;

        if (this._dataGrid.selectedNode && this._dataGrid.selectedNode.revealed)
            return this._dataGrid.selectedNode.record;

        return null;
    }

    _mouseOverDataGrid(event)
    {
        var hoveredDataGridNode = this._dataGrid.dataGridNodeFromNode(event.target);
        if (!hoveredDataGridNode)
            return;

        this._hoveredDataGridNode = hoveredDataGridNode;
        this._updateHighlight();
    }

    _mouseLeaveDataGrid(event)
    {
        this._hoveredDataGridNode = null;
        this._updateHighlight();
    }
};
