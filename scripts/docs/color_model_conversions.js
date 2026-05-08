// Static widget for Color.h's class docstring. It shows the single
// stored RGB triplet in the center, with HSV and CMYK helper views
// converting into and out of that representation.

class ColorModelConversions {
  constructor() {
    this.color = { r: 0.23, g: 0.53, b: 0.82 };
  }

  clamp01(value) {
    return Math.max(0, Math.min(1, value));
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

  swatch(topLeft, size, fill, label, value) {
    const group = new Group();
    const rect = new Rectangle(topLeft, size);
    const rectSvg = rect.toSVG();
    rectSvg.setAttribute('style', `fill: ${fill};`);
    group.add({ toSVG: () => rectSvg });
    group.add(new Text(topLeft.plus(new Vector(0.08, size.y + 0.28)), label));
    if (value) {
      group.add(new Text(topLeft.plus(new Vector(0.08, size.y + 0.62)), value));
    }
    return group;
  }

  arrow(from, to, klass) {
    return new Line(from, to.minus(from), `${klass || ''} arrow`.trim());
  }

  componentBar(origin, label, color, amount) {
    const group = new Group();
    group.add(new Text(origin.plus(new Vector(0, 0.22)), label));

    const outline = new Rectangle(origin.plus(new Vector(0.5, 0)), new Vector(1.35, 0.25));
    group.add(outline);

    const fill = new Rectangle(origin.plus(new Vector(0.5, 0)), new Vector(1.35 * amount, 0.25));
    const fillSvg = fill.toSVG();
    fillSvg.setAttribute('style', `fill: ${color}; stroke: none;`);
    group.add({ toSVG: () => fillSvg });

    group.add(new Text(origin.plus(new Vector(1.95, 0.22)), amount.toFixed(2)));
    return group;
  }

  addPanel(canvas, origin, title, rows) {
    canvas.add(new Rectangle(origin, new Vector(3.1, 2.15)));
    canvas.add(new Text(origin.plus(new Vector(0.18, 0.32)), title));
    rows.forEach((row, index) => {
      canvas.add(this.componentBar(
        origin.plus(new Vector(0.18, 0.68 + index * 0.38)),
        row.label,
        row.color,
        row.amount
      ));
    });
  }

  createCanvas() {
    const hsv = this.hsvFromRgb(this.color);
    const cmyk = this.cmykFromRgb(this.color);

    const canvas = new Canvas(520, 300);
    canvas.setTransform('translate(0, 0) scale(30, 30)');

    const rgbOrigin = new Vector(6.8, 3.8);
    const hsvOrigin = new Vector(0.35, 0.35);
    const cmykOrigin = new Vector(13.85, 0.35);

    canvas.add(new Text(new Vector(5.15, 0.55), 'Color stores RGB, helpers convert at the edges'));

    this.addPanel(canvas, hsvOrigin, 'HSV helper view', [
      { label: 'H', color: 'hsl(208, 100%, 50%)', amount: hsv.h / 360 },
      { label: 'S', color: this.rgbCss({ r: 0.2, g: 0.55, b: 0.9 }), amount: hsv.s },
      { label: 'V', color: '#555555', amount: hsv.v }
    ]);

    this.addPanel(canvas, cmykOrigin, 'CMYK helper view', [
      { label: 'C', color: '#00a7c8', amount: cmyk.c },
      { label: 'M', color: '#c0008a', amount: cmyk.m },
      { label: 'Y', color: '#d6b000', amount: cmyk.y },
      { label: 'K', color: '#333333', amount: cmyk.k }
    ]);

    canvas.add(new Rectangle(rgbOrigin.minus(new Vector(0.25, 0.6)), new Vector(3.85, 3.0)));
    canvas.add(new Text(rgbOrigin.plus(new Vector(-0.02, -0.25)), 'RGB storage'));
    canvas.add(this.swatch(rgbOrigin, new Vector(1.25, 1.0), this.rgbCss(this.color), 'stored triplet',
      `(${this.color.r.toFixed(2)}, ${this.color.g.toFixed(2)}, ${this.color.b.toFixed(2)})`));

    const barsOrigin = rgbOrigin.plus(new Vector(1.55, 0.05));
    canvas.add(this.componentBar(barsOrigin, 'R', '#d94848', this.color.r));
    canvas.add(this.componentBar(barsOrigin.plus(new Vector(0, 0.42)), 'G', '#2f9e44', this.color.g));
    canvas.add(this.componentBar(barsOrigin.plus(new Vector(0, 0.84)), 'B', '#2060d0', this.color.b));

    canvas.add(this.arrow(new Vector(3.45, 1.35), new Vector(6.65, 4.2), 'blue'));
    canvas.add(this.arrow(new Vector(6.65, 4.55), new Vector(3.45, 1.7), 'blue'));
    canvas.add(new Text(new Vector(3.75, 2.25), 'fromHSV()'));
    canvas.add(new Text(new Vector(3.42, 3.05), 'h(), s(), v()'));

    canvas.add(this.arrow(new Vector(13.75, 1.35), new Vector(10.65, 4.2), 'green'));
    canvas.add(this.arrow(new Vector(10.65, 4.55), new Vector(13.75, 1.7), 'green'));
    canvas.add(new Text(new Vector(11.0, 2.25), 'fromCMYK()'));
    canvas.add(new Text(new Vector(11.05, 3.05), 'c(), m(), y(), k()'));

    canvas.add(new Text(new Vector(5.05, 7.55),
      'RGB is compact for rendering math; HSV and CMYK are computed views.'));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const container = document.createElement('div');
  container.appendChild(new ColorModelConversions().createCanvas());
  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
