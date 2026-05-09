// Interactive widget for mesh-triangle interpolation. Drag the triangle
// vertices or the hit point; the same barycentric weights drive the
// ray-triangle inside test, UV interpolation, and smooth vertex normals.

class MeshTriangleInterpolation {
  constructor() {
    this.width = 600;
    this.height = 360;
    this.vertices = [
      { x: 96, y: 282, uv: { u: 0.0, v: 1.0 }, normal: { x: -0.50, y: -0.86 } },
      { x: 404, y: 274, uv: { u: 1.0, v: 1.0 }, normal: { x:  0.50, y: -0.86 } },
      { x: 246, y: 66,  uv: { u: 0.5, v: 0.0 }, normal: { x:  0.00, y: -1.00 } },
    ];
    this.hit = { x: 236, y: 196 };
    this.normalMode = 'smooth';
    this.vertexColors = ['#d94841', '#2f9e44', '#2b6cb0'];
    this.vertexLabels = ['p0', 'p1', 'p2'];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'mesh-triangle-interpolation-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.normalControl = new FigureSegmentedControl({
      label: 'normal',
      value: this.normalMode,
      options: [
        { label: 'Flat', value: 'flat' },
        { label: 'Smooth', value: 'smooth' },
      ],
      onChange: (value) => {
        this.normalMode = value;
        this.render();
      },
    });

    this.widget.addControl(this.normalControl.element());
    this.widget.setContent(this.canvas.element);
    this.hit = this.clampToTriangle(this.hit);
    this.render();
    return this.widget.root;
  }

  isUsableTriangle(vertices = this.vertices) {
    return Math.abs(FigureGeometry.edge(vertices[0], vertices[1], vertices[2])) > 900;
  }

  barycentric(p) {
    const [a, b, c] = this.vertices;
    return FigureGeometry.barycentric(p, a, b, c);
  }

  clampToTriangle(point) {
    const p = {
      x: FigureMath.clamp(point.x, 18, this.width - 210),
      y: FigureMath.clamp(point.y, 18, this.height - 18),
    };
    if (this.barycentric(p).inside) return p;

    const candidates = [
      FigureGeometry.closestPointOnSegment(p, this.vertices[0], this.vertices[1]),
      FigureGeometry.closestPointOnSegment(p, this.vertices[1], this.vertices[2]),
      FigureGeometry.closestPointOnSegment(p, this.vertices[2], this.vertices[0]),
    ];
    return candidates.reduce((best, candidate) => {
      const bestDistance = Vector.from(best).distanceTo(p);
      const candidateDistance = Vector.from(candidate).distanceTo(p);
      return candidateDistance < bestDistance ? candidate : best;
    });
  }

  interpolatedUV(weights) {
    const uv = { u: 0, v: 0 };
    [weights.w0, weights.w1, weights.w2].forEach((w, i) => {
      uv.u += this.vertices[i].uv.u * w;
      uv.v += this.vertices[i].uv.v * w;
    });
    return uv;
  }

  flatNormal() {
    const edge = Vector.from(this.vertices[1]).minus(this.vertices[0]);
    return edge.perp().safeNormalized(Vector.up);
  }

  smoothNormal(weights) {
    let n = Vector.null;
    [weights.w0, weights.w1, weights.w2].forEach((w, i) => {
      n = n.plus(Vector.from(this.vertices[i].normal).multiply(w));
    });
    return n.safeNormalized(Vector.up);
  }

  currentNormal(weights) {
    return this.normalMode === 'flat' ? this.flatNormal() : this.smoothNormal(weights);
  }

  polygonPoints(points) {
    return points.map(p => `${p.x},${p.y}`).join(' ');
  }

  render() {
    this.hit = this.clampToTriangle(this.hit);
    const weights = this.barycentric(this.hit);
    const uv = this.interpolatedUV(weights);
    const normal = this.currentNormal(weights);

    this.canvas.clear();
    this.renderTriangle(weights);
    this.renderVertexNormals();
    this.renderInterpolatedNormal(normal);
    this.renderHitHandle();
    this.renderVertexHandles();
    this.renderUvPanel(uv);
    this.renderReadout(weights, uv, normal);
  }

  renderTriangle(weights) {
    this.canvas.add('polygon', {
      points: this.polygonPoints(this.vertices),
      fill: '#f8f1d8',
      stroke: '#202020',
      'stroke-width': FigurePixelStrokeWidth,
    });

    const pairs = [[1, 2, 0], [2, 0, 1], [0, 1, 2]];
    pairs.forEach(([a, b, colorIndex], i) => {
      this.canvas.add('polygon', {
        points: this.polygonPoints([this.hit, this.vertices[a], this.vertices[b]]),
        fill: this.vertexColors[colorIndex],
        'fill-opacity': (0.12 + Math.max(0, [weights.w0, weights.w1, weights.w2][i]) * 0.28).toFixed(3),
        stroke: 'none',
      });
    });

    this.vertices.forEach((vertex, index) => {
      this.canvas.add('line', {
        x1: this.hit.x,
        y1: this.hit.y,
        x2: vertex.x,
        y2: vertex.y,
        stroke: this.vertexColors[index],
        'stroke-width': FigurePixelGuideStrokeWidth,
        'stroke-dasharray': '5 5',
        'pointer-events': 'none',
      });
    });
  }

  renderArrow(start, direction, length, stroke, attrs = {}) {
    const end = Vector.from(start).plus(Vector.from(direction).multiply(length));
    this.canvas.add('line', {
      x1: start.x,
      y1: start.y,
      x2: end.x,
      y2: end.y,
      stroke,
      'stroke-width': FigurePixelStrokeWidth,
      'stroke-linecap': 'round',
      ...attrs,
    });
    this.canvas.add('circle', {
      cx: end.x,
      cy: end.y,
      r: 3.5,
      fill: stroke,
      stroke,
      'stroke-width': FigurePixelGuideStrokeWidth,
      'pointer-events': 'none',
    });
  }

  renderVertexNormals() {
    this.vertices.forEach((vertex, index) => {
      this.renderArrow(vertex, vertex.normal, 42, this.vertexColors[index], {
        'data-normal-vector': 'vertex',
        'data-vertex-index': index,
        'pointer-events': 'none',
      });
    });
  }

  renderInterpolatedNormal(normal) {
    this.renderArrow(this.hit, normal, 62, '#111', {
      'data-normal-vector': this.normalMode,
      'pointer-events': 'none',
    });
  }

  renderHitHandle() {
    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.hit,
      radius: 8,
      attrs: {
        fill: '#ffffff',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'triangle-hit-point',
      },
      onDrag: (point) => {
        this.hit = this.clampToTriangle(point);
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }

  renderVertexHandles() {
    this.vertices.forEach((vertex, index) => {
      const handle = new FigureDraggablePoint({
        canvas: this.canvas,
        point: vertex,
        radius: 9,
        attrs: {
          fill: this.vertexColors[index],
          stroke: '#111',
          'stroke-width': FigurePixelStrokeWidth,
          'data-drag-handle': 'triangle-vertex',
          'data-vertex-index': index,
        },
        onDrag: (point) => {
          const next = this.vertices.map(v => ({ ...v, uv: v.uv, normal: v.normal }));
          next[index] = {
            ...next[index],
            x: FigureMath.clamp(point.x, 24, this.width - 220),
            y: FigureMath.clamp(point.y, 24, this.height - 24),
          };
          if (!this.isUsableTriangle(next)) return;
          this.vertices = next;
          this.hit = this.clampToTriangle(this.hit);
          this.render();
        },
      });
      this.canvas.append(handle.element());

      const label = this.canvas.add('text', {
        x: vertex.x + 13,
        y: vertex.y - 12,
        'font-size': 13,
        'font-family': 'monospace',
        'font-weight': 'bold',
        fill: this.vertexColors[index],
        'pointer-events': 'none',
      });
      label.textContent = `${this.vertexLabels[index]}  uv(${vertex.uv.u.toFixed(1)},${vertex.uv.v.toFixed(1)})`;
    });
  }

  renderUvPanel(uv) {
    const x = 430;
    const y = 42;
    const size = 116;
    this.canvas.add('rect', {
      x,
      y,
      width: size,
      height: size,
      fill: '#ffffff',
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
    });

    for (let i = 1; i < 4; ++i) {
      this.canvas.add('line', {
        x1: x + size * i / 4,
        y1: y,
        x2: x + size * i / 4,
        y2: y + size,
        stroke: '#d0d0d0',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
      this.canvas.add('line', {
        x1: x,
        y1: y + size * i / 4,
        x2: x + size,
        y2: y + size * i / 4,
        stroke: '#d0d0d0',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
    }

    this.vertices.forEach((vertex, index) => {
      this.canvas.add('circle', {
        cx: x + vertex.uv.u * size,
        cy: y + vertex.uv.v * size,
        r: 5,
        fill: this.vertexColors[index],
        stroke: '#111',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
    });
    this.canvas.add('circle', {
      cx: x + uv.u * size,
      cy: y + uv.v * size,
      r: 7,
      fill: '#ffffff',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      'data-interpolated-uv': '1',
    });

    const label = this.canvas.add('text', {
      x,
      y: y - 14,
      'font-size': 13,
      'font-family': 'sans-serif',
      'font-weight': 'bold',
      fill: '#222',
    });
    label.textContent = 'UV uses the same weights';
  }

  renderReadout(weights, uv, normal) {
    const x = 414;
    const y = 192;
    this.canvas.add('rect', {
      x,
      y,
      width: 172,
      height: 132,
      fill: '#fffef8',
      stroke: '#999',
      'stroke-width': FigurePixelGuideStrokeWidth,
      rx: 4,
      'pointer-events': 'none',
    });

    const fmt = n => n.toFixed(2);
    const lines = [
      `alpha = ${fmt(weights.w0)}`,
      `beta  = ${fmt(weights.w1)}`,
      `gamma = ${fmt(weights.w2)}`,
      `sum   = ${fmt(weights.w0 + weights.w1 + weights.w2)}`,
      `uv    = (${fmt(uv.u)}, ${fmt(uv.v)})`,
      `n     = (${fmt(normal.x)}, ${fmt(normal.y)})`,
    ];
    lines.forEach((line, i) => {
      const text = this.canvas.add('text', {
        x: x + 12,
        y: y + 22 + i * 18,
        'font-size': 13,
        'font-family': 'monospace',
        fill: '#222',
        'pointer-events': 'none',
      });
      text.textContent = line;
    });
  }
}

((scriptElement) => {
  const figure = new MeshTriangleInterpolation();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
