// Interactive widget for the rasterizer's MSAA coverage and resolve step.
// Each pixel is shaded by the fraction of its subpixel samples covered by a
// draggable triangle. 1x coverage is binary; 2x/4x/8x can resolve edge pixels
// to intermediate values.

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

    this.widget.addControl(this.sampleControl.element());
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

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  handleInsetCells() {
    return this.handleRadius / this.cell;
  }

  clampGridPoint(point) {
    const inset = this.handleInsetCells();
    return {
      x: this.clamp(point.x, inset, this.cols - inset),
      y: this.clamp(point.y, inset, this.rows - inset),
    };
  }

  edge(a, b, p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
  }

  insideTriangle(p) {
    const area = this.edge(this.vertices[0], this.vertices[1], this.vertices[2]);
    if (Math.abs(area) < 1e-9) return false;
    const w0 = this.edge(this.vertices[1], this.vertices[2], p);
    const w1 = this.edge(this.vertices[2], this.vertices[0], p);
    const w2 = area - w0 - w1;
    return area > 0
      ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
      : (w0 <= 0 && w1 <= 0 && w2 <= 0);
  }

  coverageColor(covered, total) {
    const t = covered / total;
    const mix = (a, b) => Math.round(a + (b - a) * t);
    return `rgb(${mix(255, 11)}, ${mix(255, 114)}, ${mix(255, 133)})`;
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
        let covered = 0;
        for (const offset of pattern) {
          if (this.insideTriangle({
            x: px + 0.5 + offset.x,
            y: py + 0.5 + offset.y,
          })) {
            covered++;
          }
        }

        this.canvas.add('rect', {
          x: px * this.cell,
          y: py * this.cell,
          width: this.cell,
          height: this.cell,
          fill: this.coverageColor(covered, pattern.length),
          stroke: '#d4d4d4',
          'stroke-width': FigurePixelGuideStrokeWidth,
          'data-covered-samples': covered,
          'data-sample-count': pattern.length,
        });

        if (covered > 0 && covered < pattern.length) {
          const label = this.canvas.add('text', {
            x: px * this.cell + this.cell / 2,
            y: py * this.cell + this.cell / 2 + 4,
            'font-family': 'monospace',
            'font-size': 11,
            'text-anchor': 'middle',
            fill: covered > pattern.length / 2 ? '#fff' : '#222',
            'pointer-events': 'none',
          });
          label.textContent = `${covered}/${pattern.length}`;
        }

        this.renderSamples(px, py, pattern);
      }
    }
  }

  renderSamples(px, py, pattern) {
    for (const offset of pattern) {
      const samplePoint = {
        x: px + 0.5 + offset.x,
        y: py + 0.5 + offset.y,
      };
      const hit = this.insideTriangle(samplePoint);
      this.canvas.add('circle', {
        cx: samplePoint.x * this.cell,
        cy: samplePoint.y * this.cell,
        r: Math.max(2.5, 6 - pattern.length * 0.35),
        fill: hit ? '#0b7285' : '#ffffff',
        stroke: hit ? '#083f4a' : '#9b9b9b',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-sample-hit': hit ? '1' : '0',
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
      `${this.sampleCount}x MSAA: resolved color = covered samples / ${this.sampleCount}`;
  }
}

((scriptElement) => {
  const figure = new RasterizerMSAACoverage();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
