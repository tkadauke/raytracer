// Interactive widget for `engine::raster::Rasterizer`'s edge-function
// rasterization (Pineda 1988). Shows the triangle, the bounding-box scan
// region, per-pixel inside tests, barycentric weights, and UV interpolation.

class RasterizerPipeline {
  constructor() {
    this.cols = 12;
    this.rows = 8;
    this.cell = 32;
    this.vertices = [
      { x: 1.5, y: 4.5 },
      { x: 7.5, y: 5.0 },
      { x: 4.0, y: 1.0 },
    ];
    this.vertexUVs = [
      { u: 0.0, v: 1.0 },
      { u: 1.0, v: 1.0 },
      { u: 0.5, v: 0.0 },
    ];
    this.vertexColors = ['#d22', '#2c2', '#26d'];
    this.vertexLabels = ['p0', 'p1', 'p2'];
    this.cursor = { x: 4.0, y: 3.5 };
    this.mode = 'barycentric';
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-pipeline-widget' });
    this.canvas = new FigureSvg({
      width: this.width(),
      height: this.height(),
      viewBox: `0 0 ${this.width()} ${this.height()}`,
    });
    this.canvas.element.style.cursor = 'crosshair';
    this.canvas.element.addEventListener('pointermove', (event) => {
      const cell = this.cellFromPointer(event);
      this.cursor = {
        x: this.clamp(cell.x, 0, this.cols),
        y: this.clamp(cell.y, 0, this.rows),
      };
      this.render();
    });

    this.modeControl = new FigureSegmentedControl({
      value: this.mode,
      options: [
        { label: 'Barycentric color', value: 'barycentric' },
        { label: 'UV color', value: 'uv' },
      ],
      onChange: (value) => {
        this.mode = value;
        this.render();
      },
    });

    this.widget.addControl(this.modeControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  width() {
    return this.cols * this.cell;
  }

  height() {
    return this.rows * this.cell;
  }

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  edge(a, b, p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
  }

  area() {
    return this.edge(this.vertices[0], this.vertices[1], this.vertices[2]);
  }

  bounds() {
    const [p0, p1, p2] = this.vertices;
    return {
      minX: Math.max(0, Math.floor(Math.min(p0.x, p1.x, p2.x))),
      maxX: Math.min(this.cols - 1, Math.ceil(Math.max(p0.x, p1.x, p2.x))),
      minY: Math.max(0, Math.floor(Math.min(p0.y, p1.y, p2.y))),
      maxY: Math.min(this.rows - 1, Math.ceil(Math.max(p0.y, p1.y, p2.y))),
    };
  }

  weightsAt(p) {
    const area = this.area();
    if (Math.abs(area) <= 0) return null;
    const w0 = this.edge(this.vertices[1], this.vertices[2], p);
    const w1 = this.edge(this.vertices[2], this.vertices[0], p);
    const w2 = area - w0 - w1;
    const inside = area > 0
      ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
      : (w0 <= 0 && w1 <= 0 && w2 <= 0);
    return {
      b0: w0 / area,
      b1: w1 / area,
      b2: w2 / area,
      inside,
    };
  }

  interpolatedUV({ b0, b1, b2 }) {
    return {
      u: this.vertexUVs[0].u * b0 + this.vertexUVs[1].u * b1 + this.vertexUVs[2].u * b2,
      v: this.vertexUVs[0].v * b0 + this.vertexUVs[1].v * b1 + this.vertexUVs[2].v * b2,
    };
  }

  fillForWeights(weights) {
    if (this.mode === 'uv') {
      const uv = this.interpolatedUV(weights);
      return `rgb(${Math.round(255 * uv.u)}, ${Math.round(255 * uv.v)}, 0)`;
    }
    return `rgb(${Math.round(255 * weights.b0)}, ${Math.round(255 * weights.b1)}, ${Math.round(255 * weights.b2)})`;
  }

  toSvgPoint(v) {
    return { x: v.x * this.cell, y: v.y * this.cell };
  }

  fromSvgPoint(point) {
    return {
      x: this.clamp(point.x / this.cell, 0, this.cols),
      y: this.clamp(point.y / this.cell, 0, this.rows),
    };
  }

  cellFromPointer(event) {
    const point = this.canvas.pointFromEvent(event);
    return {
      x: point.x / this.cell,
      y: point.y / this.cell,
    };
  }

  render() {
    this.canvas.clear();
    this.renderGrid();
    this.renderBoundingBox();
    this.renderPixels();
    this.renderTriangle();
    this.renderCursor();
  }

  renderGrid() {
    for (let y = 0; y <= this.rows; y++) {
      this.canvas.add('line', {
        x1: 0,
        y1: y * this.cell,
        x2: this.width(),
        y2: y * this.cell,
        stroke: '#ddd',
        'stroke-width': 1,
      });
    }
    for (let x = 0; x <= this.cols; x++) {
      this.canvas.add('line', {
        x1: x * this.cell,
        y1: 0,
        x2: x * this.cell,
        y2: this.height(),
        stroke: '#ddd',
        'stroke-width': 1,
      });
    }
  }

  renderBoundingBox() {
    const { minX, maxX, minY, maxY } = this.bounds();
    this.canvas.add('rect', {
      x: minX * this.cell,
      y: minY * this.cell,
      width: (maxX - minX + 1) * this.cell,
      height: (maxY - minY + 1) * this.cell,
      fill: 'none',
      stroke: '#888',
      'stroke-width': 2,
      'stroke-dasharray': '4 4',
    });
  }

  renderPixels() {
    if (Math.abs(this.area()) <= 0) return;
    const { minX, maxX, minY, maxY } = this.bounds();
    for (let py = minY; py <= maxY; py++) {
      for (let px = minX; px <= maxX; px++) {
        const weights = this.weightsAt({ x: px + 0.5, y: py + 0.5 });
        if (!weights || !weights.inside) continue;
        this.canvas.add('rect', {
          x: px * this.cell,
          y: py * this.cell,
          width: this.cell,
          height: this.cell,
          fill: this.fillForWeights(weights),
          'fill-opacity': 0.85,
        });
      }
    }
  }

  renderTriangle() {
    this.canvas.add('polygon', {
      points: this.vertices.map(v => `${v.x * this.cell},${v.y * this.cell}`).join(' '),
      fill: 'none',
      stroke: '#222',
      'stroke-width': 2.5,
    });

    this.vertices.forEach((vertex, index) => {
      const handle = new FigureDraggablePoint({
        canvas: this.canvas,
        point: this.toSvgPoint(vertex),
        radius: 9,
        attrs: {
          fill: this.vertexColors[index],
          stroke: '#000',
          'stroke-width': 2,
          'data-drag-handle': 'triangle-vertex',
          'data-vertex-index': index,
        },
        onDrag: (point) => {
          this.vertices[index] = this.fromSvgPoint(point);
          this.render();
        },
      });
      this.canvas.append(handle.element());

      const label = this.canvas.add('text', {
        x: vertex.x * this.cell + 14,
        y: vertex.y * this.cell - 10,
        'font-size': 13,
        'font-family': 'monospace',
        fill: this.vertexColors[index],
        'font-weight': 'bold',
      });
      label.textContent = this.vertexLabels[index];
    });
  }

  renderCursor() {
    const weights = this.weightsAt(this.cursor);
    if (!weights) return;
    const uv = this.interpolatedUV(weights);
    this.canvas.add('circle', {
      cx: this.cursor.x * this.cell,
      cy: this.cursor.y * this.cell,
      r: 5,
      fill: weights.inside ? '#fff' : '#fff8',
      stroke: '#000',
      'stroke-width': 2,
      'pointer-events': 'none',
    });
    this.canvas.add('rect', {
      x: 6,
      y: this.rows * this.cell - 64,
      width: 230,
      height: 58,
      fill: '#fffe',
      stroke: '#888',
      'stroke-width': 1,
      rx: 4,
      'pointer-events': 'none',
    });

    const fmt = n => (n >= 0 ? ' ' : '') + n.toFixed(2);
    [
      `w0 = ${fmt(weights.b0)}   w1 = ${fmt(weights.b1)}   w2 = ${fmt(weights.b2)}`,
      `uv = (${uv.u.toFixed(2)}, ${uv.v.toFixed(2)})  sum = ${fmt(weights.b0 + weights.b1 + weights.b2)}`,
      `${weights.inside ? 'inside' : 'outside'} the triangle`,
    ].forEach((line, i) => {
      const text = this.canvas.add('text', {
        x: 14,
        y: this.rows * this.cell - 46 + i * 16,
        'font-size': 12,
        'font-family': 'monospace',
        fill: '#222',
        'pointer-events': 'none',
      });
      text.textContent = line;
    });
  }
}

((scriptElement) => {
  const figure = new RasterizerPipeline();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
