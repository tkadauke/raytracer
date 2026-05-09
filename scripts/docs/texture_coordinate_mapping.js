// Interactive widget for 2D texture coordinate mapping.
//
// A checker texture first maps a hit point to texture-space coordinates `(s, t)`.
// The lookup then chooses a color from the parity of
// `floor(s) + floor(t)`. Planar mapping reads coordinates from the surface
// point. UV mapping reads the hit point's stored UVs and applies U/V scale.

class TextureCoordinateMapping {
  constructor() {
    this.width = 640;
    this.height = 300;
    this.surfaceSize = 230;
    this.previewCell = 22;
    this.mapping = 'planar';
    this.uScale = 3.0;
    this.vScale = 2.0;
    this.point = { x: 0.62, y: 0.36 };
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'texture-coordinate-mapping-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.mappingControl = new FigureSegmentedControl({
      label: 'mapping',
      value: this.mapping,
      options: [
        { label: 'Planar', value: 'planar' },
        { label: 'UV', value: 'uv' },
      ],
      onChange: (value) => {
        this.mapping = value;
        this.render();
      },
    });
    this.uScaleControl = new FigureSliderControl({
      label: 'U scale',
      min: 0.5,
      max: 6,
      step: 0.5,
      value: this.uScale,
      precision: 1,
      format: value => value.toFixed(1).replace(/\.0$/, ''),
      onChange: (value) => {
        this.uScale = value;
        this.render();
      },
    });
    this.vScaleControl = new FigureSliderControl({
      label: 'V scale',
      min: 0.5,
      max: 6,
      step: 0.5,
      value: this.vScale,
      precision: 1,
      format: value => value.toFixed(1).replace(/\.0$/, ''),
      onChange: (value) => {
        this.vScale = value;
        this.render();
      },
    });

    this.widget.addControl(this.mappingControl.element());
    this.widget.addControl(this.uScaleControl.element());
    this.widget.addControl(this.vScaleControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  surfaceOrigin() {
    return { x: 36, y: 38 };
  }

  surfacePoint() {
    const origin = this.surfaceOrigin();
    return {
      x: origin.x + this.point.x * this.surfaceSize,
      y: origin.y + (1.0 - this.point.y) * this.surfaceSize,
    };
  }

  fromSurfacePoint(point) {
    const origin = this.surfaceOrigin();
    return {
      x: this.clamp((point.x - origin.x) / this.surfaceSize, 0, 1),
      y: this.clamp(1.0 - (point.y - origin.y) / this.surfaceSize, 0, 1),
    };
  }

  coordinates() {
    if (this.mapping === 'uv') {
      return {
        s: this.point.x * this.uScale,
        t: this.point.y * this.vScale,
      };
    }
    return {
      s: this.point.x,
      t: this.point.y,
    };
  }

  parity() {
    const { s, t } = this.coordinates();
    const sFloor = Math.floor(s);
    const tFloor = Math.floor(t);
    return {
      sFloor,
      tFloor,
      sum: sFloor + tFloor,
      bright: (sFloor + tFloor) % 2 === 0,
    };
  }

  checkerColor(s, t) {
    return this.checkerColorName(s, t) === 'bright' ? '#f7f0c2' : '#2f4050';
  }

  checkerColorName(s, t) {
    return (Math.floor(s) + Math.floor(t)) % 2 === 0 ? 'bright' : 'dark';
  }

  addText(x, y, text, attrs = {}) {
    const element = this.canvas.add('text', {
      x,
      y,
      'font-family': 'sans-serif',
      'font-size': 14,
      fill: '#222',
      ...attrs,
    });
    element.textContent = text;
    return element;
  }

  render() {
    this.canvas.clear();
    this.renderSurface();
    this.renderMappingReadout();
    this.renderTexturePreview();
  }

  renderSurface() {
    const origin = this.surfaceOrigin();
    const size = this.surfaceSize;

    this.addText(origin.x, 22, 'Surface hit point');
    this.canvas.add('rect', {
      x: origin.x,
      y: origin.y,
      width: size,
      height: size,
      fill: '#f8fbff',
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
    });

    for (let i = 1; i < 4; i++) {
      const x = origin.x + i * size / 4;
      const y = origin.y + i * size / 4;
      this.canvas.add('line', {
        x1: x,
        y1: origin.y,
        x2: x,
        y2: origin.y + size,
        stroke: '#d7dee8',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
      this.canvas.add('line', {
        x1: origin.x,
        y1: y,
        x2: origin.x + size,
        y2: y,
        stroke: '#d7dee8',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
    }

    this.addText(origin.x + 6, origin.y + size - 8, 'planar x / u');
    this.addText(origin.x + size - 48, origin.y + 18, 'z / v');

    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.surfacePoint(),
      radius: 9,
      attrs: {
        fill: '#e03131',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'sample-point',
      },
      onDrag: (point) => {
        this.point = this.fromSurfacePoint(point);
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }

  renderMappingReadout() {
    const x = 305;
    const y = 55;
    const { s, t } = this.coordinates();
    const parity = this.parity();
    const source = this.mapping === 'uv'
      ? `uv (${this.point.x.toFixed(2)}, ${this.point.y.toFixed(2)}) x scale`
      : `point (${this.point.x.toFixed(2)}, ${this.point.y.toFixed(2)})`;

    this.addText(x, 22, 'Map, then look up');
    this.addText(x, y, `source: ${source}`, { 'font-family': 'monospace' });
    this.addText(x, y + 28, `(s, t) = (${s.toFixed(2)}, ${t.toFixed(2)})`, {
      'font-family': 'monospace',
      'data-readout': 'texture-coordinates',
    });
    this.addText(
      x,
      y + 56,
      `floor(s) + floor(t) = ${parity.sFloor} + ${parity.tFloor} = ${parity.sum}`,
      {
        'font-family': 'monospace',
        'data-readout': 'checker-parity',
      },
    );
    this.addText(x, y + 84, parity.bright ? 'even: bright texture' : 'odd: dark texture', {
      'font-family': 'monospace',
      fill: parity.bright ? '#705f00' : '#2f4050',
    });

    this.canvas.add('rect', {
      x,
      y: y + 108,
      width: 72,
      height: 44,
      fill: parity.bright ? '#f7f0c2' : '#2f4050',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      'data-sampled-color': parity.bright ? 'bright' : 'dark',
    });
  }

  renderTexturePreview() {
    const origin = { x: 486, y: 46 };
    const cells = 6;
    const { s, t } = this.coordinates();
    const sFloor = Math.floor(s);
    const tFloor = Math.floor(t);
    const marker = {
      x: origin.x + this.clamp(s, 0, cells) * this.previewCell,
      y: origin.y + (cells - this.clamp(t, 0, cells)) * this.previewCell,
    };

    this.addText(origin.x, 22, 'Sampled texture');
    for (let row = 0; row < cells; row++) {
      for (let col = 0; col < cells; col++) {
        const cellS = col;
        const cellT = cells - 1 - row;
        const colorName = this.checkerColorName(cellS, cellT);
        this.canvas.add('rect', {
          x: origin.x + col * this.previewCell,
          y: origin.y + row * this.previewCell,
          width: this.previewCell,
          height: this.previewCell,
          fill: this.checkerColor(cellS, cellT),
          stroke: '#ffffff',
          'stroke-width': FigurePixelGuideStrokeWidth,
          'data-preview-cell': `${col},${row}`,
          'data-preview-coordinate': `${cellS},${cellT}`,
          'data-preview-color': colorName,
        });
      }
    }

    this.canvas.add('rect', {
      x: origin.x,
      y: origin.y,
      width: cells * this.previewCell,
      height: cells * this.previewCell,
      fill: 'none',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
    });
    this.canvas.add('circle', {
      cx: marker.x,
      cy: marker.y,
      r: 6,
      fill: '#e03131',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      'pointer-events': 'none',
      'data-preview-sample': 'point',
      'data-preview-sample-cell': `${sFloor},${tFloor}`,
      'data-preview-sample-color': this.checkerColorName(s, t),
    });
  }
}

((scriptElement) => {
  const figure = new TextureCoordinateMapping();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
