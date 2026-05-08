// Interactive widget for Camera forward projection and homogeneous clip space.
//
// Drag the world point across the view plane and behind the camera. Pinhole
// mode shows the perspective divide: the projected pixel is x/w, and w is the
// signed eye-relative depth. Orthographic mode keeps w at 1 because parallel
// projection has no perspective divisor.

class CameraForwardProjection {
  constructor() {
    this.width = 560;
    this.height = 300;
    this.eye = { x: 96, y: 150 };
    this.viewX = 210;
    this.nearX = 126;
    this.planeHalfHeight = 90;
    this.point = { x: 350, y: 88 };
    this.mode = 'pinhole';
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'camera-forward-projection-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.modeControl = new FigureSegmentedControl({
      label: 'camera',
      value: this.mode,
      options: [
        { label: 'Pinhole', value: 'pinhole' },
        { label: 'Orthographic', value: 'orthographic' },
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

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  signedDepth() {
    return this.mode === 'pinhole'
      ? (this.point.x - this.eye.x) / 45
      : (this.point.x - this.viewX) / 45;
  }

  clipW() {
    return this.mode === 'pinhole' ? this.signedDepth() : 1;
  }

  projection() {
    if (this.mode === 'orthographic') {
      return {
        x: this.viewX,
        y: this.point.y,
        visible: this.signedDepth() >= 0,
      };
    }

    const denominator = this.point.x - this.eye.x;
    if (Math.abs(denominator) < 0.001) {
      return {
        x: this.viewX,
        y: this.point.y < this.eye.y ? -10000 : 10000,
        visible: false,
      };
    }

    const t = (this.viewX - this.eye.x) / denominator;
    const y = this.eye.y + (this.point.y - this.eye.y) * t;
    return {
      x: this.viewX,
      y,
      visible: denominator > 0,
    };
  }

  normalizedClipY() {
    const depth = this.signedDepth();
    if (this.mode === 'orthographic') {
      return (this.point.y - this.eye.y) / this.planeHalfHeight;
    }
    if (Math.abs(depth) < 0.001) return Infinity;
    return ((this.point.y - this.eye.y) / this.planeHalfHeight) / depth;
  }

  clipCoordinates() {
    const depth = this.signedDepth();
    const w = this.clipW();
    const clipY = this.mode === 'pinhole'
      ? (this.point.y - this.eye.y) / this.planeHalfHeight
      : this.normalizedClipY();
    return {
      x: 0,
      y: clipY,
      z: depth,
      w,
      ndcY: this.normalizedClipY(),
    };
  }

  format(value) {
    if (!Number.isFinite(value)) return 'inf';
    return value.toFixed(2).replace(/^-0\.00$/, '0.00');
  }

  addText(x, y, text, attrs = {}) {
    const element = this.canvas.add('text', {
      x,
      y,
      'font-family': attrs.monospace ? 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace' : 'sans-serif',
      'font-size': attrs.size || 13,
      fill: attrs.fill || '#222',
      'font-weight': attrs.bold ? 'bold' : undefined,
      'text-anchor': attrs.anchor,
    });
    element.textContent = text;
    return element;
  }

  drawScene() {
    const planeTop = this.eye.y - this.planeHalfHeight;
    const planeBottom = this.eye.y + this.planeHalfHeight;
    const projection = this.projection();
    const depth = this.signedDepth();
    const visible = depth >= 0 && projection.y >= planeTop && projection.y <= planeBottom;

    this.canvas.add('rect', {
      x: 24,
      y: 24,
      width: 376,
      height: 252,
      fill: '#fff',
      stroke: '#ddd',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });

    this.canvas.add('line', {
      x1: this.nearX,
      y1: 34,
      x2: this.nearX,
      y2: 266,
      stroke: '#c77',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '5 5',
    });
    this.addText(this.nearX + 6, 48, 'near clip', { size: 12, fill: '#944' });

    this.canvas.add('line', {
      x1: this.viewX,
      y1: planeTop,
      x2: this.viewX,
      y2: planeBottom,
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
    });
    this.addText(this.viewX + 10, planeTop - 8, 'view plane', { bold: true });

    if (this.mode === 'pinhole') {
      this.canvas.add('path', {
        d: `M ${this.eye.x} ${this.eye.y - 12} L ${this.eye.x + 20} ${this.eye.y} L ${this.eye.x} ${this.eye.y + 12} Z`,
        fill: '#333',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
      });
      this.addText(this.eye.x - 8, this.eye.y + 30, 'eye', { anchor: 'end' });
      this.canvas.add('line', {
        x1: this.eye.x,
        y1: this.eye.y,
        x2: this.point.x,
        y2: this.point.y,
        stroke: visible ? '#2060d0' : '#999',
        'stroke-width': FigurePixelStrokeWidth,
        'stroke-dasharray': visible ? undefined : '5 5',
      });
    } else {
      this.canvas.add('path', {
        d: `M ${this.eye.x - 10} ${planeTop + 16} L ${this.eye.x + 18} ${planeTop + 16} L ${this.eye.x + 18} ${planeBottom - 16} L ${this.eye.x - 10} ${planeBottom - 16} Z`,
        fill: '#333',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
      });
      this.addText(this.eye.x + 4, planeBottom + 20, 'parallel rays', { anchor: 'middle' });
      [-42, 0, 42].forEach((offset) => {
        this.canvas.add('line', {
          x1: this.eye.x + 24,
          y1: this.point.y + offset,
          x2: this.point.x,
          y2: this.point.y + offset,
          stroke: offset === 0 ? '#2060d0' : '#9fb8e6',
          'stroke-width': offset === 0 ? FigurePixelStrokeWidth : FigurePixelGuideStrokeWidth,
        });
      });
    }

    this.canvas.add('line', {
      x1: this.point.x,
      y1: this.point.y,
      x2: projection.x,
      y2: projection.y,
      stroke: '#20a050',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '5 5',
      'pointer-events': 'none',
    });

    this.canvas.add('circle', {
      cx: projection.x,
      cy: this.clamp(projection.y, planeTop, planeBottom),
      r: 8,
      fill: visible ? '#ffdb4d' : '#fff7cc',
      stroke: visible ? '#111' : '#999',
      'stroke-width': FigurePixelStrokeWidth,
      'data-projected-pixel': 'true',
    });
    this.addText(projection.x + 12, this.clamp(projection.y, planeTop + 16, planeBottom - 10), 'pixel');

    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.point,
      radius: 9,
      attrs: {
        fill: '#e94b35',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'world-point',
      },
      onDrag: (point) => {
        this.point = {
          x: this.clamp(point.x, 46, 388),
          y: this.clamp(point.y, 42, 258),
        };
        this.render();
      },
    });
    this.canvas.append(handle.element());
    this.addText(this.point.x + 12, this.point.y - 12, 'world point', { bold: true, fill: '#9d2417' });
  }

  drawClipPanel() {
    const clip = this.clipCoordinates();
    const panelX = 420;

    this.canvas.add('rect', {
      x: panelX,
      y: 24,
      width: 116,
      height: 252,
      fill: '#fff',
      stroke: '#ddd',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.addText(panelX + 8, 48, 'clip space', { bold: true });
    this.addText(panelX + 8, 78, `depth z = ${this.format(clip.z)}`, { monospace: true });
    this.addText(panelX + 8, 100, `w = ${this.format(clip.w)}`, { monospace: true });
    this.addText(panelX + 8, 122, `y = ${this.format(clip.y)}`, { monospace: true });
    this.addText(panelX + 8, 144, `y/w = ${this.format(clip.ndcY)}`, { monospace: true });

    const midX = panelX + 58;
    const top = 172;
    const bottom = 254;
    this.canvas.add('line', {
      x1: midX,
      y1: top,
      x2: midX,
      y2: bottom,
      stroke: '#888',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.canvas.add('line', {
      x1: midX - 26,
      y1: top,
      x2: midX + 26,
      y2: top,
      stroke: '#888',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.canvas.add('line', {
      x1: midX - 26,
      y1: bottom,
      x2: midX + 26,
      y2: bottom,
      stroke: '#888',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.addText(midX + 32, top + 4, '-1', { size: 12, fill: '#666' });
    this.addText(midX + 32, bottom + 4, '1', { size: 12, fill: '#666' });

    const ndcY = this.clamp(clip.ndcY, -1.4, 1.4);
    const markerY = top + ((ndcY + 1) / 2) * (bottom - top);
    this.canvas.add('circle', {
      cx: midX,
      cy: markerY,
      r: 7,
      fill: clip.z >= 0 && Math.abs(clip.ndcY) <= 1 ? '#ffdb4d' : '#fff7cc',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
    });
  }

  render() {
    this.canvas.clear();
    this.drawScene();
    this.drawClipPanel();
  }
}

((scriptElement) => {
  const figure = new CameraForwardProjection();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
