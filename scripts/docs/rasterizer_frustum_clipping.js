// Interactive widget for view-frustum clipping. The clipping camera stays
// fixed; the reader orbits an independent inspection view around the result.

class RasterizerFrustumClipping {
  constructor() {
    this.width = 640;
    this.height = 390;
    this.yaw = -34;
    this.pitch = 20;
    this.near = 1.0;
    this.far = 4.0;
    this.scale = 70;
    this.center = { x: this.width * 0.50, y: this.height * 0.54 };
    this.vertices = [
      { x: -2.45, y: -0.95, z: 1.30 },
      { x: 1.75, y: 1.05, z: 2.05 },
      { x: 0.45, y: -0.55, z: 4.80 },
    ];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-frustum-clipping-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.widget.addControl(new FigureSliderControl({
      label: 'inspection yaw',
      min: -80,
      max: 80,
      value: this.yaw,
      step: 1,
      precision: 0,
      format: v => `${v.toFixed(0)} deg`,
      onChange: (value) => {
        this.yaw = value;
        this.render();
      },
    }).element());

    this.widget.addControl(new FigureSliderControl({
      label: 'inspection pitch',
      min: -35,
      max: 55,
      value: this.pitch,
      step: 1,
      precision: 0,
      format: v => `${v.toFixed(0)} deg`,
      onChange: (value) => {
        this.pitch = value;
        this.render();
      },
    }).element());

    this.widget.addControl(new FigureSliderControl({
      label: 'near depth',
      min: 0.55,
      max: 2.1,
      value: this.near,
      step: 0.05,
      precision: 2,
      onChange: (value) => {
        this.near = value;
        this.render();
      },
    }).element());

    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  degToRad(degrees) {
    return degrees * Math.PI / 180.0;
  }

  frustumHalfSize(z) {
    return { x: z * 0.80, y: z * 0.58 };
  }

  frustumCorners() {
    const near = this.frustumHalfSize(this.near);
    const far = this.frustumHalfSize(this.far);
    return {
      near: [
        { x: -near.x, y: -near.y, z: this.near },
        { x: near.x, y: -near.y, z: this.near },
        { x: near.x, y: near.y, z: this.near },
        { x: -near.x, y: near.y, z: this.near },
      ],
      far: [
        { x: -far.x, y: -far.y, z: this.far },
        { x: far.x, y: -far.y, z: this.far },
        { x: far.x, y: far.y, z: this.far },
        { x: -far.x, y: far.y, z: this.far },
      ],
    };
  }

  planeDistance(point, plane) {
    if (plane === 'near') return point.z - this.near;
    if (plane === 'far') return this.far - point.z;
    if (plane === 'left') return point.x + point.z * 0.80;
    if (plane === 'right') return point.z * 0.80 - point.x;
    if (plane === 'bottom') return point.y + point.z * 0.58;
    return point.z * 0.58 - point.y;
  }

  interpolate(a, b, t, generated = true) {
    return {
      x: FigureMath.lerp(a.x, b.x, t),
      y: FigureMath.lerp(a.y, b.y, t),
      z: FigureMath.lerp(a.z, b.z, t),
      generated,
    };
  }

  clipAgainst(poly, plane) {
    if (poly.length === 0) return [];
    const out = [];
    let prev = poly[poly.length - 1];
    let prevDistance = this.planeDistance(prev, plane);
    for (const curr of poly) {
      const currDistance = this.planeDistance(curr, plane);
      const prevInside = prevDistance >= 0;
      const currInside = currDistance >= 0;
      if (prevInside !== currInside) {
        const t = prevDistance / (prevDistance - currDistance);
        out.push(this.interpolate(prev, curr, t));
      }
      if (currInside) out.push(curr);
      prev = curr;
      prevDistance = currDistance;
    }
    return out;
  }

  clippedPolygon() {
    let polygon = this.vertices.map(v => ({ ...v, generated: false }));
    ['near', 'left', 'right', 'bottom', 'top', 'far'].forEach((plane) => {
      polygon = this.clipAgainst(polygon, plane);
    });
    return polygon;
  }

  project(point) {
    const yaw = this.degToRad(this.yaw);
    const pitch = this.degToRad(this.pitch);
    const cosY = Math.cos(yaw);
    const sinY = Math.sin(yaw);
    const cosX = Math.cos(pitch);
    const sinX = Math.sin(pitch);
    const x1 = point.x * cosY - point.z * sinY;
    const z1 = point.x * sinY + point.z * cosY;
    const y1 = point.y * cosX - z1 * sinX;
    return {
      x: this.center.x + x1 * this.scale,
      y: this.center.y - y1 * this.scale,
    };
  }

  points(points) {
    return points.map(p => {
      const projected = this.project(p);
      return `${projected.x.toFixed(1)},${projected.y.toFixed(1)}`;
    }).join(' ');
  }

  line3(a, b, attrs = {}) {
    this.canvas.line(this.project(a), this.project(b), attrs);
  }

  render() {
    this.canvas.clear();
    this.renderFrustum();
    this.renderSourceTriangle();
    this.renderClippedPolygon();
    this.renderClipVertices();
    this.renderCamera();
    this.renderLegend();
  }

  renderFrustum() {
    const corners = this.frustumCorners();
    const edgeAttrs = {
      stroke: '#2b2b2b',
      'stroke-width': FigurePixelStrokeWidth,
      fill: 'none',
    };
    [corners.near, corners.far].forEach((quad) => {
      for (let i = 0; i < 4; i++) this.line3(quad[i], quad[(i + 1) % 4], edgeAttrs);
    });
    for (let i = 0; i < 4; i++) {
      this.line3(corners.near[i], corners.far[i], {
        ...edgeAttrs,
        stroke: '#666',
        'stroke-dasharray': '5 5',
      });
    }
    this.canvas.add('polygon', {
      points: this.points(corners.near),
      fill: '#ffd166',
      'fill-opacity': 0.18,
      stroke: '#7a4f00',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'data-frustum-near-plane': 'true',
    });
    this.canvas.add('polygon', {
      points: this.points(corners.far),
      fill: '#a0c4ff',
      'fill-opacity': 0.12,
      stroke: '#355070',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'data-frustum-far-plane': 'true',
    });
  }

  renderSourceTriangle() {
    this.canvas.add('polygon', {
      points: this.points(this.vertices),
      fill: 'none',
      stroke: '#8d8d8d',
      'stroke-width': FigurePixelStrokeWidth,
      'stroke-dasharray': '8 5',
      'data-source-geometry': 'true',
    });
    this.vertices.forEach((vertex, index) => {
      const p = this.project(vertex);
      this.canvas.add('circle', {
        cx: p.x,
        cy: p.y,
        r: 5,
        fill: '#ffffff',
        stroke: '#555',
        'stroke-width': FigurePixelStrokeWidth,
        'data-source-vertex': String(index),
      });
    });
  }

  renderClippedPolygon() {
    const clipped = this.clippedPolygon();
    if (clipped.length < 3) return;
    this.canvas.add('polygon', {
      points: this.points(clipped),
      fill: '#2a9d8f',
      'fill-opacity': 0.42,
      stroke: '#006d77',
      'stroke-width': FigurePixelStrokeWidth,
      'data-clipped-output': 'true',
    });
  }

  renderClipVertices() {
    this.clippedPolygon().forEach((vertex) => {
      if (!vertex.generated) return;
      const p = this.project(vertex);
      this.canvas.add('rect', {
        x: p.x - 5,
        y: p.y - 5,
        width: 10,
        height: 10,
        fill: '#ffb703',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-generated-clip-vertex': 'true',
      });
    });
  }

  renderCamera() {
    const eye = { x: 0, y: 0, z: 0 };
    this.canvas.add('circle', {
      ...this.project(eye),
      r: 5,
      fill: '#111',
      stroke: '#111',
      'data-clipping-camera': 'true',
    });
    this.line3(eye, { x: 0, y: 0, z: this.near }, {
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
    });
  }

  renderLegend() {
    const entries = [
      ['#8d8d8d', 'source triangle'],
      ['#2a9d8f', 'clipped output'],
      ['#ffb703', 'generated vertices'],
    ];
    entries.forEach(([color, label], index) => {
      const y = 24 + index * 22;
      this.canvas.add('rect', {
        x: 16,
        y: y - 10,
        width: 14,
        height: 14,
        fill: color,
        'fill-opacity': label === 'source triangle' ? 0.25 : 0.75,
        stroke: '#202020',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
      this.canvas.text(38, y + 2, label, { 'font-size': 13 });
    });
  }
}

((scriptElement) => {
  const figure = new RasterizerFrustumClipping();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
