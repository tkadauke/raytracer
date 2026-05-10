// Interactive widget for textbook chapter 24 — Shape classification.
// A draggable polygon on the left; live readouts of the descriptors and
// the ShapeClassifier verdict on the right. Deform the polygon to push
// the descriptors across the classifier's decision boundaries and watch
// the verdict flip.

class ShapeDescriptors {
  constructor() {
    this.width = 540;
    this.height = 320;
    this.padding = 24;
    this.canvasArea = {
      x: this.padding,
      y: this.padding,
      width: 280,
      height: this.height - 2 * this.padding,
    };
    this.readoutX = this.canvasArea.x + this.canvasArea.width + 24;

    // Default polygon: an irregular pentagon. Drag the vertices to deform it.
    const cx = this.canvasArea.x + this.canvasArea.width / 2;
    const cy = this.canvasArea.y + this.canvasArea.height / 2;
    const r = 100;
    this.vertices = [];
    for (let i = 0; i < 5; i++) {
      const t = (i / 5) * Math.PI * 2 - Math.PI / 2;
      this.vertices.push({ x: cx + r * Math.cos(t), y: cy + r * Math.sin(t) });
    }

    // Classifier thresholds, copied from test/helpers/ShapeClassifier.cpp
    // so the visible verdict matches the C++ predicate exactly.
    this.thresholds = {
      circleRadialVarianceMax: 0.10,
      circleAspectMin: 0.83,
      circleAspectMax: 1.20,
      rectangleRadialVarianceMin: 0.10,
      rectangleRadialVarianceMax: 0.30,
    };
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'shape-descriptors-widget' });
    this.widget.setControlsVisible(false);
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  render() {
    this.canvas.clear();
    this.renderPolygonArea();
    this.renderPolygon();
    this.renderHandles();
    this.renderReadout();
  }

  renderPolygonArea() {
    this.canvas.add('rect', {
      x: this.canvasArea.x,
      y: this.canvasArea.y,
      width: this.canvasArea.width,
      height: this.canvasArea.height,
      fill: '#fafafa',
      stroke: '#d0d0d0',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
  }

  renderPolygon() {
    const points = this.vertices.map(v => `${v.x},${v.y}`).join(' ');
    this.canvas.add('polygon', {
      points,
      fill: '#cce5ff',
      'fill-opacity': 0.7,
      stroke: '#1f77b4',
      'stroke-width': 2,
    });

    // Centroid marker.
    const c = this.centroid();
    this.canvas.add('circle', {
      cx: c.x,
      cy: c.y,
      r: 3,
      fill: '#1f77b4',
    });
  }

  renderHandles() {
    this.vertices.forEach((vertex, index) => {
      const handle = new FigureDraggablePoint({
        canvas: this.canvas,
        point: { x: vertex.x, y: vertex.y },
        radius: 7,
        attrs: {
          fill: '#ff7f0e',
          stroke: '#000',
          'stroke-width': FigurePixelStrokeWidth,
          'data-drag-handle': 'shape-vertex',
          'data-vertex-index': index,
        },
        onDrag: (point) => {
          this.vertices[index] = {
            x: this.clamp(point.x, this.canvasArea.x + 4, this.canvasArea.x + this.canvasArea.width - 4),
            y: this.clamp(point.y, this.canvasArea.y + 4, this.canvasArea.y + this.canvasArea.height - 4),
          };
          this.render();
        },
      });
      this.canvas.append(handle.element());
    });
  }

  renderReadout() {
    const desc = this.descriptors();
    const verdict = this.classify(desc);

    const lines = [
      ['Descriptors', null, '#222', 'bold'],
      [`area`, desc.area.toFixed(0), '#444', 'normal'],
      [`perimeter`, desc.perimeter.toFixed(1), '#444', 'normal'],
      [`bbox`, `${desc.bbox.width.toFixed(0)} x ${desc.bbox.height.toFixed(0)}`, '#444', 'normal'],
      [`aspect ratio`, desc.aspectRatio.toFixed(3), '#444', 'normal'],
      [`radial variance`, desc.radialVariance.toFixed(3), '#444', 'normal'],
      ['', null, null, null],
      ['Classifier', null, '#222', 'bold'],
      [`isCircle`, verdict.isCircle ? 'true' : 'false',
        verdict.isCircle ? '#2ca02c' : '#888', 'bold'],
      [`isRectangle`, verdict.isRectangle ? 'true' : 'false',
        verdict.isRectangle ? '#2ca02c' : '#888', 'bold'],
      ['', null, null, null],
      ['Thresholds', null, '#222', 'bold'],
      [`circle var <`, this.thresholds.circleRadialVarianceMax.toFixed(2), '#666', 'normal'],
      [`circle aspect`,
        `[${this.thresholds.circleAspectMin.toFixed(2)}, ${this.thresholds.circleAspectMax.toFixed(2)}]`,
        '#666', 'normal'],
      [`rect var`,
        `[${this.thresholds.rectangleRadialVarianceMin.toFixed(2)}, ${this.thresholds.rectangleRadialVarianceMax.toFixed(2)}]`,
        '#666', 'normal'],
    ];

    let y = this.canvasArea.y + 14;
    const labelX = this.readoutX;
    const valueX = this.readoutX + 130;
    for (const [label, value, color, weight] of lines) {
      if (label === '') {
        y += 8;
        continue;
      }
      const labelEl = this.canvas.add('text', {
        x: labelX,
        y,
        'font-family': 'monospace',
        'font-size': 12,
        fill: color,
        'font-weight': weight,
      });
      labelEl.textContent = label;
      if (value !== null) {
        const valueEl = this.canvas.add('text', {
          x: valueX,
          y,
          'font-family': 'monospace',
          'font-size': 12,
          fill: color,
          'font-weight': weight,
        });
        valueEl.textContent = value;
      }
      y += 18;
    }
  }

  centroid() {
    const sx = this.vertices.reduce((acc, v) => acc + v.x, 0);
    const sy = this.vertices.reduce((acc, v) => acc + v.y, 0);
    const n = this.vertices.length;
    return { x: sx / n, y: sy / n };
  }

  // Polygon area via the shoelace formula (absolute value).
  polygonArea() {
    let sum = 0;
    const n = this.vertices.length;
    for (let i = 0; i < n; i++) {
      const a = this.vertices[i];
      const b = this.vertices[(i + 1) % n];
      sum += a.x * b.y - b.x * a.y;
    }
    return Math.abs(sum) / 2;
  }

  polygonPerimeter() {
    let sum = 0;
    const n = this.vertices.length;
    for (let i = 0; i < n; i++) {
      const a = this.vertices[i];
      const b = this.vertices[(i + 1) % n];
      sum += Math.hypot(b.x - a.x, b.y - a.y);
    }
    return sum;
  }

  boundingBox() {
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const v of this.vertices) {
      if (v.x < minX) minX = v.x;
      if (v.x > maxX) maxX = v.x;
      if (v.y < minY) minY = v.y;
      if (v.y > maxY) maxY = v.y;
    }
    return { x: minX, y: minY, width: maxX - minX, height: maxY - minY };
  }

  // Match Silhouette::radialVariance: standard deviation of the
  // vertex-to-centroid distance, normalised by the mean.
  radialVariance() {
    const c = this.centroid();
    const distances = this.vertices.map(v => Math.hypot(v.x - c.x, v.y - c.y));
    const mean = distances.reduce((a, b) => a + b, 0) / distances.length;
    if (mean === 0) return 0;
    const variance = distances.reduce((acc, d) => acc + (d - mean) ** 2, 0) / distances.length;
    return Math.sqrt(variance) / mean;
  }

  // Match Silhouette::aspectRatio: bounding-box height divided by width.
  aspectRatio() {
    const bbox = this.boundingBox();
    if (bbox.width === 0) return Infinity;
    return bbox.height / bbox.width;
  }

  descriptors() {
    return {
      area: this.polygonArea(),
      perimeter: this.polygonPerimeter(),
      bbox: this.boundingBox(),
      aspectRatio: this.aspectRatio(),
      radialVariance: this.radialVariance(),
    };
  }

  // Match ShapeClassifier::isCircle / isRectangle exactly.
  classify(desc) {
    const isCircle =
      desc.radialVariance < this.thresholds.circleRadialVarianceMax &&
      desc.aspectRatio >= this.thresholds.circleAspectMin &&
      desc.aspectRatio <= this.thresholds.circleAspectMax;
    const isRectangle =
      desc.radialVariance >= this.thresholds.rectangleRadialVarianceMin &&
      desc.radialVariance <= this.thresholds.rectangleRadialVarianceMax;
    return { isCircle, isRectangle };
  }

  clamp(v, min, max) {
    return Math.max(min, Math.min(max, v));
  }
}

((scriptElement) => {
  const figure = new ShapeDescriptors();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
