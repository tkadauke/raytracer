// Interactive widget for homogeneous/screen clipping preserving interpolated
// attributes. Drag any source vertex handle; each generated clip vertex keeps
// linearly interpolated UVs.

class RasterizerClipAttributes {
  constructor() {
    this.width = 560;
    this.height = 300;
    this.clip = { left: 145, top: 54, right: 430, bottom: 246 };
    this.vertices = [
      { x: 42,  y: 150, u: 0.00, v: 0.50, generated: false },
      { x: 360, y: 42,  u: 1.00, v: 0.00, generated: false },
      { x: 384, y: 262, u: 1.00, v: 1.00, generated: false },
    ];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-clip-widget' });
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

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  uvColor(v) {
    return `rgb(${Math.round(255 * v.u)}, ${Math.round(255 * v.v)}, 0)`;
  }

  interpolate(a, b, t, generated) {
    return {
      x: a.x + (b.x - a.x) * t,
      y: a.y + (b.y - a.y) * t,
      u: a.u + (b.u - a.u) * t,
      v: a.v + (b.v - a.v) * t,
      generated,
    };
  }

  clipAgainst(poly, inside, intersect) {
    if (poly.length === 0) return [];
    const out = [];
    let prev = poly[poly.length - 1];
    let prevInside = inside(prev);
    for (const curr of poly) {
      const currInside = inside(curr);
      if (currInside !== prevInside) {
        out.push(intersect(prev, curr));
      }
      if (currInside) out.push(curr);
      prev = curr;
      prevInside = currInside;
    }
    return out;
  }

  clippedPolygon() {
    let p = this.vertices;
    p = this.clipAgainst(
      p,
      v => v.x >= this.clip.left,
      (a, b) => this.interpolate(a, b, (this.clip.left - a.x) / (b.x - a.x), true));
    p = this.clipAgainst(
      p,
      v => v.x <= this.clip.right,
      (a, b) => this.interpolate(a, b, (this.clip.right - a.x) / (b.x - a.x), true));
    p = this.clipAgainst(
      p,
      v => v.y >= this.clip.top,
      (a, b) => this.interpolate(a, b, (this.clip.top - a.y) / (b.y - a.y), true));
    p = this.clipAgainst(
      p,
      v => v.y <= this.clip.bottom,
      (a, b) => this.interpolate(a, b, (this.clip.bottom - a.y) / (b.y - a.y), true));
    return p;
  }

  polygonPoints(vertices) {
    return vertices.map(v => `${v.x},${v.y}`).join(' ');
  }

  render() {
    this.canvas.clear();
    const clipped = this.clippedPolygon();
    this.renderClipRect();
    this.renderOriginalTriangle();
    this.renderClippedTriangle(clipped);
    this.renderSourceVertices();
    this.renderGeneratedVertices(clipped);
  }

  renderClipRect() {
    this.canvas.add('rect', {
      x: this.clip.left,
      y: this.clip.top,
      width: this.clip.right - this.clip.left,
      height: this.clip.bottom - this.clip.top,
      fill: '#fff',
      stroke: '#333',
      'stroke-width': FigurePixelStrokeWidth,
    });

    const label = this.canvas.add('text', {
      x: this.clip.left + 8,
      y: this.clip.top + 18,
      'font-family': 'sans-serif',
      'font-size': 13,
      fill: '#333',
    });
    label.textContent = 'viewport clip rectangle';
  }

  renderOriginalTriangle() {
    this.canvas.add('polygon', {
      points: this.polygonPoints(this.vertices),
      fill: 'none',
      stroke: '#999',
      'stroke-width': FigurePixelStrokeWidth,
      'stroke-dasharray': '7 5',
    });
  }

  renderClippedTriangle(clipped) {
    if (clipped.length < 3) return;
    this.canvas.add('polygon', {
      points: this.polygonPoints(clipped),
      fill: '#83c5be',
      'fill-opacity': 0.25,
      stroke: '#0b7285',
      'stroke-width': FigurePixelStrokeWidth,
    });
  }

  renderSourceVertices() {
    this.vertices.forEach((vertex, index) => {
      const handle = new FigureDraggablePoint({
        canvas: this.canvas,
        point: vertex,
        radius: 8,
        attrs: {
          fill: this.uvColor(vertex),
          stroke: '#111',
          'stroke-width': FigurePixelStrokeWidth,
          'data-drag-handle': 'source-vertex',
          'data-vertex-index': index,
        },
        onDrag: (point) => {
          vertex.x = this.clamp(point.x, 0, this.width);
          vertex.y = this.clamp(point.y, 0, this.height);
          this.render();
        },
      });
      this.canvas.append(handle.element());
    });
  }

  renderGeneratedVertices(clipped) {
    clipped.forEach((v) => {
      if (!v.generated) return;
      this.canvas.add('rect', {
        x: v.x - 6,
        y: v.y - 6,
        width: 12,
        height: 12,
        fill: this.uvColor(v),
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-generated-clip-vertex': '1',
      });
    });
  }
}

((scriptElement) => {
  const figure = new RasterizerClipAttributes();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
