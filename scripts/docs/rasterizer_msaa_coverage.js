// Interactive widget for the rasterizer's MSAA coverage, shading, and resolve
// step. A draggable triangle controls which subpixel samples are covered. The
// shading mode controls whether each covered sample uses its own interpolated
// color or reuses one shaded color for the whole triangle/pixel pair.

class RasterizerMSAACoverage {
  constructor() {
    this.cols = 9;
    this.rows = 6;
    this.cell = 48;
    this.captionHeight = 34;
    this.handleRadius = 8;
    this.samplePatterns = {
      1: [{ x: 0.0, y: 0.0 }],
      2: [
        { x: -0.25, y: -0.25 },
        { x:  0.25, y:  0.25 },
      ],
      4: [
        { x: -0.125, y: -0.375 },
        { x:  0.375, y: -0.125 },
        { x: -0.375, y:  0.125 },
        { x:  0.125, y:  0.375 },
      ],
      8: [
        { x:  0.0625, y: -0.1875 },
        { x: -0.0625, y:  0.1875 },
        { x:  0.3125, y:  0.0625 },
        { x: -0.1875, y: -0.3125 },
        { x: -0.3125, y:  0.3125 },
        { x: -0.4375, y: -0.0625 },
        { x:  0.1875, y:  0.4375 },
        { x:  0.4375, y: -0.4375 },
      ],
    };
    this.handleColors = ['#2f9e44', '#f03e3e', '#f59f00'];
    this.sampleCount = 4;
    this.shadingMode = 'per_sample';
    this.backgroundColor = { r: 255, g: 255, b: 255 };
    this.vertices = [
      { x: 1.0, y: 5.35 },
      { x: 8.35, y: 4.65 },
      { x: 1.25, y: 0.65 },
    ];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-msaa-widget' });
    this.canvas = new FigureSvg({
      width: this.width(),
      height: this.height(),
      viewBox: `0 0 ${this.width()} ${this.height()}`,
    });

    this.sampleControl = new FigureSegmentedControl({
      label: 'samples',
      value: this.sampleCount,
      options: [1, 2, 4, 8].map(count => ({ label: `${count}x`, value: count })),
      onChange: (value) => {
        this.sampleCount = Number(value);
        this.render();
      },
    });

    this.shadingControl = new FigureSegmentedControl({
      label: 'shading',
      value: this.shadingMode,
      options: [
        { label: 'per sample', value: 'per_sample' },
        { label: 'per fragment', value: 'per_fragment' },
      ],
      onChange: (value) => {
        this.shadingMode = value;
        this.render();
      },
    });

    this.widget.addControl(this.sampleControl.element());
    this.widget.addControl(this.shadingControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  width() {
    return this.cols * this.cell;
  }

  height() {
    return this.rows * this.cell + this.captionHeight;
  }

  handleInsetCells() {
    return this.handleRadius / this.cell;
  }

  clampGridPoint(point) {
    const inset = this.handleInsetCells();
    return {
      x: FigureMath.clamp(point.x, inset, this.cols - inset),
      y: FigureMath.clamp(point.y, inset, this.rows - inset),
    };
  }

  insideTriangle(p) {
    return FigureGeometry.pointInTriangleTopLeft(
      p, this.vertices[0], this.vertices[1], this.vertices[2]);
  }

  sampleShadeColor(p) {
    const weights = FigureGeometry.barycentricTopLeft(
      p, this.vertices[0], this.vertices[1], this.vertices[2]);
    const w0 = FigureMath.clamp01(weights.w0);
    const w1 = FigureMath.clamp01(weights.w1);
    const w2 = FigureMath.clamp01(weights.w2);
    return {
      r: 235 * w0 + 35 * w1 + 35 * w2,
      g: 55 * w0 + 165 * w1 + 85 * w2,
      b: 55 * w0 + 70 * w1 + 225 * w2,
    };
  }

  colorCss(color) {
    return `rgb(${Math.round(color.r)}, ${Math.round(color.g)}, ${Math.round(color.b)})`;
  }

  colorLuminance(color) {
    return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
  }

  resolvedPixel(px, py, pattern) {
    const samples = pattern.map((offset) => {
      const point = {
        x: px + 0.5 + offset.x,
        y: py + 0.5 + offset.y,
      };
      const hit = this.insideTriangle(point);
      return {
        point,
        hit,
        shadeColor: hit ? this.sampleShadeColor(point) : this.backgroundColor,
      };
    });

    const firstHit = samples.find(sample => sample.hit);
    const sharedShadeColor = firstHit ? firstHit.shadeColor : this.backgroundColor;
    const covered = samples.filter(sample => sample.hit).length;
    const total = { r: 0, g: 0, b: 0 };

    samples.forEach((sample) => {
      if (!sample.hit) {
        sample.resolvedColor = this.backgroundColor;
        sample.shadingSource = false;
      } else if (this.shadingMode === 'per_fragment') {
        sample.resolvedColor = sharedShadeColor;
        sample.shadingSource = sample === firstHit;
      } else {
        sample.resolvedColor = sample.shadeColor;
        sample.shadingSource = true;
      }
      total.r += sample.resolvedColor.r;
      total.g += sample.resolvedColor.g;
      total.b += sample.resolvedColor.b;
    });

    return {
      covered,
      samples,
      color: {
        r: total.r / samples.length,
        g: total.g / samples.length,
        b: total.b / samples.length,
      },
    };
  }

  toSvgPoint(v) {
    return { x: v.x * this.cell, y: v.y * this.cell };
  }

  fromSvgPoint(p) {
    return this.clampGridPoint({
      x: p.x / this.cell,
      y: p.y / this.cell,
    });
  }

  trianglePoints() {
    return this.vertices.map(v => `${v.x * this.cell},${v.y * this.cell}`).join(' ');
  }

  render() {
    this.canvas.clear();
    this.renderCoverage();
    this.renderTriangle();
    this.renderCaption();
  }

  renderCoverage() {
    const pattern = this.samplePatterns[this.sampleCount];

    for (let py = 0; py < this.rows; py++) {
      for (let px = 0; px < this.cols; px++) {
        const resolved = this.resolvedPixel(px, py, pattern);
        const covered = resolved.covered;

        this.canvas.add('rect', {
          x: px * this.cell,
          y: py * this.cell,
          width: this.cell,
          height: this.cell,
          fill: this.colorCss(resolved.color),
          stroke: '#d4d4d4',
          'stroke-width': FigurePixelGuideStrokeWidth,
          'data-covered-samples': covered,
          'data-sample-count': pattern.length,
          'data-shading-mode': this.shadingMode,
          'data-resolved-color': this.colorCss(resolved.color),
        });

        if (covered > 0 && covered < pattern.length) {
          const label = this.canvas.add('text', {
            x: px * this.cell + this.cell / 2,
            y: py * this.cell + this.cell / 2 + 4,
            'font-family': 'monospace',
            'font-size': 11,
            'text-anchor': 'middle',
            fill: this.colorLuminance(resolved.color) < 135 ? '#fff' : '#222',
            'pointer-events': 'none',
          });
          label.textContent = `${covered}/${pattern.length}`;
        }

        this.renderSamples(resolved.samples, pattern);
      }
    }
  }

  renderSamples(samples, pattern) {
    for (const sample of samples) {
      const source = sample.hit && sample.shadingSource;
      this.canvas.add('circle', {
        cx: sample.point.x * this.cell,
        cy: sample.point.y * this.cell,
        r: Math.max(2.5, 6 - pattern.length * 0.35),
        fill: sample.hit ? this.colorCss(sample.resolvedColor) : '#ffffff',
        stroke: source ? '#111111' : (sample.hit ? '#516173' : '#9b9b9b'),
        'stroke-width': source ? FigurePixelStrokeWidth : FigurePixelGuideStrokeWidth,
        'data-sample-hit': sample.hit ? '1' : '0',
        'data-shading-source': source ? '1' : '0',
        'data-shading-mode': this.shadingMode,
      });
    }
  }

  renderTriangle() {
    this.canvas.add('polygon', {
      points: this.trianglePoints(),
      fill: '#0b7285',
      'fill-opacity': 0.08,
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      'stroke-linejoin': 'round',
    });

    this.vertices.forEach((vertex, index) => {
      const handle = new FigureDraggablePoint({
        canvas: this.canvas,
        point: this.toSvgPoint(vertex),
        radius: this.handleRadius,
        attrs: {
          fill: this.handleColors[index],
          stroke: '#111',
          'stroke-width': FigurePixelStrokeWidth,
          'data-drag-handle': 'triangle-vertex',
          'data-vertex-index': index,
          'data-drag-bounds': 'pixel-grid',
        },
        onDrag: (point) => {
          this.vertices[index] = this.fromSvgPoint(point);
          this.render();
        },
      });
      this.canvas.append(handle.element());
    });
  }

  renderCaption() {
    const caption = this.canvas.add('text', {
      x: 0,
      y: this.rows * this.cell + 23,
      'font-family': 'sans-serif',
      'font-size': 14,
      fill: '#333',
    });
    caption.textContent =
      `${this.sampleCount}x MSAA, ${this.shadingMode.replace('_', ' ')}: ` +
      (this.shadingMode === 'per_fragment'
        ? 'covered samples reuse one triangle/pixel color'
        : 'covered samples shade independently');
  }
}

((scriptElement) => {
  const figure = new RasterizerMSAACoverage();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
