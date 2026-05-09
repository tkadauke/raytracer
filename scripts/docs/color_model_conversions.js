// Static widget for Color.h's class docstring. The point is deliberately
// narrow: RGB is the stored representation, while HSV and CMYK are helper
// views computed at the API edges.

class ColorModelConversions {
  constructor() {
    this.width = 520;
    this.height = 356;
    this.color = { r: 0.23, g: 0.53, b: 0.82 };
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'color-model-conversions-widget' });
    this.widget.setControlsVisible(false);
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`
    });
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  clamp01(value) {
    return FigureMath.clamp01(value);
  }

  rgbCss(color) {
    const r = Math.round(this.clamp01(color.r) * 255);
    const g = Math.round(this.clamp01(color.g) * 255);
    const b = Math.round(this.clamp01(color.b) * 255);
    return `rgb(${r}, ${g}, ${b})`;
  }

  hsvFromRgb(color) {
    const max = Math.max(color.r, color.g, color.b);
    const min = Math.min(color.r, color.g, color.b);
    const delta = max - min;
    let h = 0;

    if (delta !== 0) {
      if (max === color.r) {
        h = 60 * (((color.g - color.b) / delta) % 6);
      } else if (max === color.g) {
        h = 60 * (((color.b - color.r) / delta) + 2);
      } else {
        h = 60 * (((color.r - color.g) / delta) + 4);
      }
    }

    if (h < 0) h += 360;

    return {
      h: Math.round(h),
      s: max === 0 ? 0 : delta / max,
      v: max
    };
  }

  cmykFromRgb(color) {
    const k = 1 - Math.max(color.r, color.g, color.b);
    const w = 1 - k;
    if (w === 0) return { c: 0, m: 0, y: 0, k: 1 };

    return {
      c: (w - color.r) / w,
      m: (w - color.g) / w,
      y: (w - color.b) / w,
      k: k
    };
  }

  addPanel(x, y, width, height, title) {
    this.canvas.panel({ x, y, width, height }, title);
  }

  addComponentBar(x, y, label, color, amount, valueText = amount.toFixed(2)) {
    const trackX = x + 22;
    const trackWidth = 72;
    const trackHeight = 10;
    this.canvas.text(x, y + 12, label, {
      'font-size': 13,
      'font-weight': 700
    });
    this.canvas.add('rect', {
      x: trackX,
      y,
      width: trackWidth,
      height: trackHeight,
      fill: '#ffffff',
      stroke: '#202020',
      'stroke-width': FigurePixelGuideStrokeWidth
    });
    this.canvas.add('rect', {
      x: trackX,
      y,
      width: trackWidth * this.clamp01(amount),
      height: trackHeight,
      fill: color,
      stroke: 'none',
      'data-component': label
    });
    this.canvas.text(trackX + trackWidth + 8, y + 12, valueText, {
      'font-size': 13,
      'font-variant-numeric': 'tabular-nums'
    });
  }

  renderRgbPanel() {
    this.addPanel(22, 78, 240, 260, 'RGB storage');

    this.canvas.add('rect', {
      x: 42,
      y: 114,
      width: 72,
      height: 72,
      fill: this.rgbCss(this.color),
      stroke: '#202020',
      'stroke-width': FigurePixelStrokeWidth,
      'data-color-model': 'rgb-swatch'
    });

    this.addComponentBar(130, 116, 'R', '#d94848', this.color.r);
    this.addComponentBar(130, 142, 'G', '#2f9e44', this.color.g);
    this.addComponentBar(130, 168, 'B', '#2060d0', this.color.b);

    this.canvas.text(42, 211, 'stored triplet', {
      'font-size': 13,
      'font-weight': 600
    });
    this.canvas.text(42, 231,
      `(${this.color.r.toFixed(2)}, ${this.color.g.toFixed(2)}, ${this.color.b.toFixed(2)})`, {
        'font-size': 13,
        fill: '#555555'
      });
  }

  renderHelperPanel() {
    const hsv = this.hsvFromRgb(this.color);
    const cmyk = this.cmykFromRgb(this.color);

    this.addPanel(282, 78, 216, 260, 'Helper views');

    this.canvas.text(302, 132, 'HSV helper view', {
      'font-size': 14,
      'font-weight': 700
    });
    this.canvas.text(304, 149, 'fromHSV()', {
      'font-size': 12,
      fill: '#555555'
    });
    this.addComponentBar(304, 162, 'H', 'hsl(208, 100%, 50%)', hsv.h / 360, `${hsv.h} deg`);
    this.addComponentBar(304, 177, 'S', '#3b82f6', hsv.s);
    this.addComponentBar(304, 192, 'V', '#555555', hsv.v);

    this.canvas.text(302, 233, 'CMYK helper view', {
      'font-size': 14,
      'font-weight': 700
    });
    this.canvas.text(304, 250, 'fromCMYK()', {
      'font-size': 12,
      fill: '#555555'
    });
    this.addComponentBar(304, 263, 'C', '#00a7c8', cmyk.c);
    this.addComponentBar(304, 278, 'M', '#c0008a', cmyk.m);
    this.addComponentBar(304, 293, 'Y', '#d6b000', cmyk.y);
    this.addComponentBar(304, 308, 'K', '#333333', cmyk.k);
  }

  render() {
    this.canvas.clear();

    this.canvas.text(22, 34, 'Color is stored once as RGB', {
      'font-size': 22,
      'font-weight': 700
    });
    this.canvas.text(22, 56, 'HSV and CMYK are conversion helpers, not separate stored state.', {
      'font-size': 13,
      fill: '#555555'
    });

    this.renderRgbPanel();

    this.renderHelperPanel();
  }
}

((scriptElement) => {
  const widget = new ColorModelConversions();
  scriptElement.parentNode.appendChild(widget.element());
})(document.currentScript);
