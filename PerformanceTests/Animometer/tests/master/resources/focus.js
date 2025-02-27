(function() {

var maxVerticalOffset = 50;
var radius = 10;
var centerDiameter = 100;
var sizeVariance = 80;
var travelDistance = 70;

var minObjectDepth = 0.2;
var maxObjectDepth = 1.0;

var opacityMultiplier = 30;

var FocusElement = Utilities.createClass(
    function(stage)
    {
        var topOffset = maxVerticalOffset * Stage.randomSign();
        var top = Stage.random(0, stage.size.height - 2 * radius - sizeVariance);
        var left = Stage.random(0, stage.size.width - 2 * radius - sizeVariance);

        // size and blurring are a function of depth
        this._depth = Utilities.lerp(1 - Math.pow(Pseudo.random(), 2), minObjectDepth, maxObjectDepth);
        var distance = Utilities.lerp(this._depth, 1, sizeVariance);
        var size = 2 * radius + sizeVariance - distance;

        this.container = document.createElement('div');
        this.container.style.width = (size + stage.maxBlurValue * 2) + "px";
        this.container.style.height = (size + stage.maxBlurValue * 2) + "px";
        this.container.style.top = top + "px";
        this.container.style.left = left + "px";
        this.container.style.zIndex = Math.round((1 - this._depth) * 10);

        var particle = Utilities.createElement("div", {}, this.container);
        particle.style.width = size + "px";
        particle.style.height = size + "px";
        particle.style.top = stage.maxBlurValue + "px";
        particle.style.left = stage.maxBlurValue + "px";

        var depthMultiplier = Utilities.lerp(1 - this._depth, 0.8, 1);
        this._sinMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier;
        this._cosMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier;
    }, {

    hide: function()
    {
        this.container.style.display = "none";
    },

    show: function()
    {
        this.container.style.display = "block";
    },

    animate: function(stage, sinFactor, cosFactor)
    {
        var top = sinFactor * this._sinMultiplier;
        var left = cosFactor * this._cosMultiplier;

        Utilities.setElementPrefixedProperty(this.container, "filter", "blur(" + stage.getBlurValue(this._depth) + "px) opacity(" + stage.getOpacityValue(this._depth) + "%)");
        this.container.style.transform = "translate3d(" + left + "%, " + top + "%, 0)";
    }
});

var FocusStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
    }, {

    movementDuration: 2500,
    focusDuration: 1000,

    centerObjectDepth: 0.0,

    minBlurValue: 1.5,
    maxBlurValue: 15,
    maxCenterObjectBlurValue: 5,

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);

        this._testElements = [];
        this._focalPoint = 0.5;
        this._offsetIndex = 0;

        this._centerElement = document.getElementById("center-text");
        this._centerElement.style.width = (centerDiameter + this.maxCenterObjectBlurValue * 2) + "px";
        this._centerElement.style.height = (centerDiameter + this.maxCenterObjectBlurValue * 2) + "px";
        this._centerElement.style.zIndex = Math.round(10 * this.centerObjectDepth);

        var particle = document.querySelector("#center-text div");
        particle.style.width = centerDiameter + "px";
        particle.style.height = centerDiameter + "px";
        particle.style.top = this.maxCenterObjectBlurValue + "px";
        particle.style.left = this.maxCenterObjectBlurValue + "px";

        var blur = this.getBlurValue(this.centerObjectDepth, true);
        Utilities.setElementPrefixedProperty(this._centerElement, "filter", "blur(" + blur + "px)");
    },

    complexity: function()
    {
        return 1 + this._offsetIndex;
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this._offsetIndex = Math.max(0, this._offsetIndex + count);
            for (var i = this._offsetIndex; i < this._testElements.length; ++i)
                this._testElements[i].hide();
            return;
        }

        var newIndex = this._offsetIndex + count;
        for (var i = this._testElements.length; i < newIndex; ++i) {
            var obj = new FocusElement(this);
            this._testElements.push(obj);
            this.element.appendChild(obj.container);
        }
        for (var i = this._offsetIndex; i < newIndex; ++i)
            this._testElements[i].show();
        this._offsetIndex = newIndex;
    },

    animate: function()
    {
        var time = this._benchmark.timestamp;
        var sinFactor = Math.sin(time / this.movementDuration) * travelDistance;
        var cosFactor = Math.cos(time / this.movementDuration) * travelDistance;

        var focusProgress = 0.5 + 0.5 * Math.sin(time / this.focusDuration);
        this._focalPoint = focusProgress;

        // update center element before loop
        Utilities.setElementPrefixedProperty(this._centerElement, "filter", "blur(" + this.getBlurValue(this.centerObjectDepth, true) + "px)");

        for (var i = 0; i < this._offsetIndex; ++i)
            this._testElements[i].animate(this, sinFactor, cosFactor);
    },

    getBlurValue: function(depth, isCenter)
    {
        if (isCenter)
            return 1 + Math.abs(depth - this._focalPoint) * (this.maxCenterObjectBlurValue - 1);

        return Utilities.lerp(Math.abs(depth - this._focalPoint), this.minBlurValue, this.maxBlurValue);
    },

    getOpacityValue: function(depth)
    {
        return Math.max(1, opacityMultiplier * (1 - Math.abs(depth - this._focalPoint)));
    },
});

var FocusBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new FocusStage(), options);
    }
);

window.benchmarkClass = FocusBenchmark;

}());
