// Interactive widget for rasterizer shadow-map cascades.
//
// The production rasterizer splits the scene bounds by camera depth, blends
// linear and logarithmic split positions, builds a tighter directional-light
// shadow map for each slice, and snaps each cascade center to the light-space
// texel grid. This 2D view shows those ideas together: depth slices in the
// camera view, light-space coverage per cascade, and raw-vs-snapped map centers
// under small camera pans.

class RasterizerShadowCascades {
  constructor() {
    this.width = 760;
    this.height = 430;
    this.cameraPanel = { x: 32, y: 56, width: 448, height: 286 };
    this.mapPanel = { x: 516, y: 56, width: 210, height: 286 };
    this.readoutPanel = { x: 32, y: 354, width: 694, height: 48 };
    this.cascadeCount = 4;
    this.splitLambda = 0.5;
    this.pan = 0.0;
    this.snapCenters = true;
    this.mapTexels = 12;
    this.sceneDepthMin = 42;
    this.sceneDepthMax = 244;
    this.sceneHalfWidth = 74;
    this.lightForward = new Vector(0.88, -0.48).normalized();
    this.lightRight = new Vector(this.lightForward.y, -this.lightForward.x).normalized();
    this.colors = ['#1c7ed6', '#2f9e44', '#f08c00', '#9c36b5'];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-shadow-cascades-widget' });
    this.countControl = new FigureSegmentedControl({
      label: 'cascade count',
      value: this.cascadeCount,
      options: [1, 2, 4].map(count => ({ label: `${count}`, value: count })),
      onChange: (value) => {
        this.cascadeCount = Number(value);
        this.render();
      },
    });
    this.snapControl = new FigureSegmentedControl({
      label: 'center snap',
      value: 'on',
      options: [
        { label: 'on', value: 'on' },
        { label: 'off', value: 'off' },
      ],
      onChange: (value) => {
        this.snapCenters = value === 'on';
        this.render();
      },
    });
    this.splitControl = new FigureSliderControl({
      label: 'split blend',
      min: 0,
      max: 1,
      step: 0.05,
      value: this.splitLambda,
      precision: 2,
      onChange: (value) => {
        this.splitLambda = value;
        this.render();
      },
    });
    this.panControl = new FigureSliderControl({
      label: 'camera pan',
      min: -1,
      max: 1,
      step: 0.01,
      value: this.pan,
      precision: 2,
      onChange: (value) => {
        this.pan = value;
        this.render();
      },
    });

    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });
    this.widget.addControl(this.countControl.element());
    this.widget.addControl(this.splitControl.element());
    this.widget.addControl(this.snapControl.element());
    this.widget.addControl(this.panControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  cameraBounds() {
    const lateralCenter = this.pan * 34;
    return {
      minDepth: this.sceneDepthMin,
      maxDepth: this.sceneDepthMax,
      minLateral: lateralCenter - this.sceneHalfWidth,
      maxLateral: lateralCenter + this.sceneHalfWidth,
    };
  }

  depthRanges() {
    const bounds = this.cameraBounds();
    const span = bounds.maxDepth - bounds.minDepth;
    const ranges = [];
    let minDepth = bounds.minDepth;
    for (let index = 0; index < this.cascadeCount; ++index) {
      const ratio = (index + 1) / this.cascadeCount;
      const linear = bounds.minDepth + span * ratio;
      const logarithmic = bounds.minDepth * Math.pow(bounds.maxDepth / bounds.minDepth, ratio);
      const maxDepth = index + 1 === this.cascadeCount
        ? bounds.maxDepth
        : linear * (1 - this.splitLambda) + logarithmic * this.splitLambda;
      ranges.push({
        index,
        minDepth,
        maxDepth,
        minLateral: bounds.minLateral,
        maxLateral: bounds.maxLateral,
      });
      minDepth = maxDepth;
    }
    return ranges;
  }

  worldPoint(depth, lateral) {
    return new Vector(depth, lateral);
  }

  lightSpace(point) {
    return {
      s: point.dot(this.lightRight),
      depth: point.dot(this.lightForward),
    };
  }

  cascadeMetrics(range) {
    const corners = [
      this.worldPoint(range.minDepth, range.minLateral),
      this.worldPoint(range.maxDepth, range.minLateral),
      this.worldPoint(range.minDepth, range.maxLateral),
      this.worldPoint(range.maxDepth, range.maxLateral),
    ].map(point => this.lightSpace(point));
    const sMin = Math.min(...corners.map(point => point.s));
    const sMax = Math.max(...corners.map(point => point.s));
    const rawCenter = (sMin + sMax) / 2;
    const halfExtent = Math.max(1.0, (sMax - sMin) / 2) * 1.05;
    const texelSize = (halfExtent * 2) / this.mapTexels;
    const snappedCenter = Math.round(rawCenter / texelSize) * texelSize;
    const activeCenter = this.snapCenters ? snappedCenter : rawCenter;
    return {
      ...range,
      sMin,
      sMax,
      rawCenter,
      snappedCenter,
      activeCenter,
      halfExtent,
      texelSize,
    };
  }

  metrics() {
    return this.depthRanges().map(range => this.cascadeMetrics(range));
  }

  render() {
    this.canvas.clear();
    this.canvas.element.setAttribute('data-cascade-count', this.cascadeCount);
    this.canvas.element.setAttribute('data-cascade-split', this.splitLambda.toFixed(2));
    this.canvas.element.setAttribute('data-center-snap', this.snapCenters ? 'on' : 'off');

    const metrics = this.metrics();
    this.renderCameraPanel(metrics);
    this.renderMapPanel(metrics);
    this.renderReadout(metrics);
  }

  renderCameraPanel(metrics) {
    this.canvas.panel(this.cameraPanel, 'camera-depth splits');

    const plot = {
      x: this.cameraPanel.x + 42,
      y: this.cameraPanel.y + 54,
      width: this.cameraPanel.width - 78,
      height: this.cameraPanel.height - 104,
    };
    const bounds = this.cameraBounds();
    const lateralMin = bounds.minLateral - 34;
    const lateralMax = bounds.maxLateral + 34;
    const xForDepth = depth =>
      plot.x + (depth / (this.sceneDepthMax + 36)) * plot.width;
    const yForLateral = lateral =>
      plot.y + (1 - (lateral - lateralMin) / (lateralMax - lateralMin)) * plot.height;

    this.canvas.arrow(new Vector(plot.x - 20, plot.y + plot.height / 2),
                      new Vector(plot.x + plot.width + 18, plot.y + plot.height / 2), {
      stroke: '#333',
      'stroke-width': FigurePixelGuideStrokeWidth,
      markerId: 'cascade-depth-axis',
      markerColor: '#333',
    });
    this.canvas.text(plot.x + plot.width - 44, plot.y + plot.height / 2 - 12, 'depth', {
      'font-size': 13,
      fill: '#333',
    });

    for (const metric of metrics) {
      const color = this.colors[metric.index % this.colors.length];
      const x0 = xForDepth(metric.minDepth);
      const x1 = xForDepth(metric.maxDepth);
      const y0 = yForLateral(metric.maxLateral);
      const y1 = yForLateral(metric.minLateral);
      this.canvas.add('rect', {
        x: x0,
        y: y0,
        width: x1 - x0,
        height: y1 - y0,
        fill: color,
        'fill-opacity': 0.15,
        stroke: color,
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-camera-cascade': metric.index + 1,
      });
      this.canvas.text(x0 + 8, y0 + 20, `C${metric.index + 1}`, {
        'font-size': 14,
        'font-weight': 700,
        fill: color,
      });
      if (metric.index > 0) {
        this.canvas.line(new Vector(x0, y0 - 10), new Vector(x0, y1 + 10), {
          stroke: color,
          'stroke-width': FigurePixelStrokeWidth,
          'stroke-dasharray': '5 4',
        });
      }
    }

    this.renderSceneObjects(xForDepth, yForLateral);
    this.canvas.text(plot.x, this.cameraPanel.y + this.cameraPanel.height - 24,
                     'split blend moves detail toward near depth', {
      'font-size': 12,
      fill: '#444',
    });
  }

  renderSceneObjects(xForDepth, yForLateral) {
    const objects = [
      { depth: 74, lateral: -28 + this.pan * 34, size: 24, fill: '#e03131' },
      { depth: 132, lateral: 30 + this.pan * 34, size: 30, fill: '#1c7ed6' },
      { depth: 212, lateral: -16 + this.pan * 34, size: 26, fill: '#f08c00' },
    ];
    for (const object of objects) {
      this.canvas.add('circle', {
        cx: xForDepth(object.depth),
        cy: yForLateral(object.lateral),
        r: object.size / 2,
        fill: object.fill,
        stroke: '#111',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
    }
  }

  renderMapPanel(metrics) {
    this.canvas.panel(this.mapPanel, 'light-space maps');

    const maxHalfExtent = Math.max(...metrics.map(metric => metric.halfExtent));
    const fullWidth = maxHalfExtent * 2;
    const labelX = this.mapPanel.x + 14;
    const left = this.mapPanel.x + 56;
    const top = this.mapPanel.y + 80;
    const barWidth = this.mapPanel.width - 76;
    const barHeight = 30;
    const rowGap = 43;

    this.canvas.line(new Vector(left, this.mapPanel.y + 50),
                     new Vector(left + 18, this.mapPanel.y + 50), {
      stroke: '#e03131',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '4 4',
    });
    this.canvas.text(left + 24, this.mapPanel.y + 54, 'raw', {
      'font-size': 11,
      fill: '#444',
    });
    this.canvas.line(new Vector(left + 68, this.mapPanel.y + 50),
                     new Vector(left + 86, this.mapPanel.y + 50), {
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
    });
    this.canvas.text(left + 92, this.mapPanel.y + 54, 'snapped', {
      'font-size': 11,
      fill: '#444',
    });

    metrics.forEach((metric) => {
      const color = this.colors[metric.index % this.colors.length];
      const y = top + metric.index * rowGap;
      const width = barWidth * (metric.halfExtent * 2) / fullWidth;
      const x = left + (barWidth - width) / 2;
      const centerX = x + width / 2;
      this.canvas.add('rect', {
        x,
        y,
        width,
        height: barHeight,
        fill: color,
        'fill-opacity': 0.13,
        stroke: color,
        'stroke-width': FigurePixelStrokeWidth,
        'data-light-map-cascade': metric.index + 1,
      });

      for (let i = 1; i < this.mapTexels; ++i) {
        const tx = x + width * i / this.mapTexels;
        this.canvas.line(new Vector(tx, y), new Vector(tx, y + barHeight), {
          stroke: '#fff',
          'stroke-width': FigurePixelGuideStrokeWidth,
        });
      }

      const offsetScale = width / (metric.halfExtent * 2);
      const rawX = centerX + (metric.rawCenter - metric.activeCenter) * offsetScale;
      const snappedX = centerX + (metric.snappedCenter - metric.activeCenter) * offsetScale;
      this.canvas.line(new Vector(rawX, y - 8), new Vector(rawX, y + barHeight + 8), {
        stroke: '#e03131',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'stroke-dasharray': '4 4',
        'data-raw-center': metric.index + 1,
      });
      this.canvas.line(new Vector(snappedX, y - 8), new Vector(snappedX, y + barHeight + 8), {
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-snapped-center': metric.index + 1,
      });
      this.canvas.text(labelX, y + 20, `C${metric.index + 1}`, {
        'font-size': 13,
        'font-weight': 700,
        fill: color,
      });
    });
  }

  renderReadout(metrics) {
    this.canvas.panel(this.readoutPanel, '', { rx: 4 });
    const maxSnap = Math.max(...metrics.map(metric =>
      Math.abs(metric.snappedCenter - metric.rawCenter)));
    const texel = metrics.length > 0 ? metrics[0].texelSize : 0;
    const state = this.snapCenters ? 'snapped' : 'raw';
    this.canvas.text(this.readoutPanel.x + 18, this.readoutPanel.y + 30,
                     `${this.cascadeCount} cascade(s), split ${this.splitLambda.toFixed(2)}, ${state} centers`, {
      'font-size': 14,
      fill: '#333',
    });
    this.canvas.text(this.readoutPanel.x + 430, this.readoutPanel.y + 30,
                     `max correction ${maxSnap.toFixed(1)}, first texel ${texel.toFixed(1)}`, {
      'font-size': 14,
      fill: '#333',
    });
  }
}

((scriptElement) => {
  const widget = new RasterizerShadowCascades();
  scriptElement.parentNode.appendChild(widget.element());
})(document.currentScript);
