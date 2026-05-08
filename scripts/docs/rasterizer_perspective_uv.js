// Interactive widget for perspective-correct UV interpolation.
//
// The same projected planar quad is drawn twice. The left panel linearly spaces
// UV grid lines in screen space; the right panel projects those same UV grid
// lines from the 3D quad, matching the rasterizer's 1/z correction:
//
//   uv_pixel = (sum_i w_i * uv_i / z_i) / (sum_i w_i / z_i)
//
// Moving the depth slider pushes the right edge away from the camera. Affine
// interpolation keeps UVs evenly spaced on the projected quad, while the
// perspective-correct version compresses texture space with depth.

class RasterizerPerspectiveUV {
  constructor() {
    this.panelWidth = 280;
    this.panelHeight = 220;
    this.gap = 24;
    this.depth = 3.2;
    this.gridSteps = [0.2, 0.4, 0.6, 0.8];
    this.samplesPerLine = 28;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-perspective-uv-widget' });
    this.canvas = new FigureSvg({
      width: this.width(),
      height: this.panelHeight,
      viewBox: `0 0 ${this.width()} ${this.panelHeight}`,
    });

    this.depthControl = new FigureSliderControl({
      label: 'right edge depth',
      min: 1.05,
      max: 6,
      step: 0.1,
      value: this.depth,
      precision: 1,
      format: value => value.toFixed(1).replace(/\.0$/, ''),
      onChange: (value) => {
        this.depth = value;
        this.render();
      },
    });

    this.widget.addControl(this.depthControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  width() {
    return this.panelWidth * 2 + this.gap;
  }

  lerp(a, b, t) {
    return a + (b - a) * t;
  }

  interpolatePoint(a, b, t) {
    return {
      x: this.lerp(a.x, b.x, t),
      y: this.lerp(a.y, b.y, t),
    };
  }

  makeProjection(rightDepth) {
    const halfWidth = 1.0;
    const halfHeight = 0.62;
    const leftDepth = 1.0;
    const vertices3 = [
      { x: -halfWidth, y:  halfHeight, z: leftDepth },
      { x:  halfWidth, y:  halfHeight, z: rightDepth },
      { x:  halfWidth, y: -halfHeight, z: rightDepth },
      { x: -halfWidth, y: -halfHeight, z: leftDepth },
    ];
    const projected = vertices3.map(p => ({ x: p.x / p.z, y: p.y / p.z }));
    const xs = projected.map(p => p.x);
    const ys = projected.map(p => p.y);
    const minX = Math.min(...xs);
    const maxX = Math.max(...xs);
    const minY = Math.min(...ys);
    const maxY = Math.max(...ys);
    const scale = Math.min(190 / (maxX - minX), 130 / (maxY - minY));
    const midX = (minX + maxX) * 0.5;
    const midY = (minY + maxY) * 0.5;

    const toPanel = (p) => ({
      x: this.panelWidth * 0.5 + (p.x - midX) * scale,
      y: 116 + (p.y - midY) * scale,
    });

    const vertices = projected.map(toPanel);
    return {
      vertices,
      affinePoint: (u, v) => {
        const top = this.interpolatePoint(vertices[3], vertices[2], u);
        const bottom = this.interpolatePoint(vertices[0], vertices[1], u);
        return this.interpolatePoint(top, bottom, v);
      },
      perspectivePoint: (u, v) => {
        const point3 = {
          x: this.lerp(-halfWidth, halfWidth, u),
          y: this.lerp(-halfHeight, halfHeight, v),
          z: this.lerp(leftDepth, rightDepth, u),
        };
        return toPanel({ x: point3.x / point3.z, y: point3.y / point3.z });
      },
    };
  }

  pathForLine(pointAt) {
    const points = [];
    for (let i = 0; i <= this.samplesPerLine; ++i) {
      points.push(pointAt(i / this.samplesPerLine));
    }
    return points
      .map((point, i) => `${i === 0 ? 'M' : 'L'} ${point.x.toFixed(2)} ${point.y.toFixed(2)}`)
      .join(' ');
  }

  offsetPoint(offsetX, point) {
    return { x: offsetX + point.x, y: point.y };
  }

  drawGridLine(offsetX, pointAt, stroke, width) {
    this.canvas.add('path', {
      d: this.pathForLine(t => this.offsetPoint(offsetX, pointAt(t))),
      fill: 'none',
      stroke,
      'stroke-width': width,
      'stroke-linecap': 'round',
      'stroke-linejoin': 'round',
    });
  }

  polygonPoints(vertices, offsetX) {
    return vertices.map(v => `${offsetX + v.x},${v.y}`).join(' ');
  }

  renderPanel(projection, mode, offsetX, title) {
    const titleElement = this.canvas.add('text', {
      x: offsetX + 8,
      y: 18,
      'font-family': 'sans-serif',
      'font-size': 14,
      'font-weight': 'bold',
      fill: '#222',
    });
    titleElement.textContent = title;

    const point = mode === 'affine'
      ? projection.affinePoint
      : projection.perspectivePoint;

    this.canvas.add('polygon', {
      points: this.polygonPoints(projection.vertices, offsetX),
      fill: '#f7f3d0',
      'fill-opacity': 0.78,
    });

    for (const t of this.gridSteps) {
      this.drawGridLine(offsetX, u => point(t, u), '#9dc0c4', FigurePixelStrokeWidth * 3);
      this.drawGridLine(offsetX, u => point(u, t), '#9dc0c4', FigurePixelStrokeWidth * 3);
    }
    for (const t of this.gridSteps) {
      this.drawGridLine(offsetX, u => point(t, u), '#246b8f', FigurePixelStrokeWidth);
      this.drawGridLine(offsetX, u => point(u, t), '#246b8f', FigurePixelStrokeWidth);
    }

    this.canvas.add('polygon', {
      points: this.polygonPoints(projection.vertices, offsetX),
      fill: 'none',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
    });
  }

  render() {
    this.canvas.clear();
    const projection = this.makeProjection(this.depth);
    this.renderPanel(projection, 'affine', 0, 'Affine screen-space UV');
    this.renderPanel(
      projection,
      'perspective',
      this.panelWidth + this.gap,
      'Perspective-correct UV',
    );
  }
}

((scriptElement) => {
  const figure = new RasterizerPerspectiveUV();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
