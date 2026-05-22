((scriptElement) => {
  class InterpolationSmoothStepWidget {
    constructor() {
      this.t = 0.35;
      this.width = 560;
      this.height = 290;
      this.plot = { x: 64, y: 34, width: 420, height: 190 };
    }

    element() {
      if (this.widget) return this.widget.root;

      this.widget = new FigureWidget({ className: 'interpolation-smoothstep-widget' });
      this.tControl = new FigureSliderControl({
        label: 't',
        min: 0,
        max: 1,
        step: 0.01,
        value: this.t,
        precision: 2,
        onChange: (value) => {
          this.t = FigureMath.clamp01(value);
          this.render();
        },
      });
      this.canvas = new FigureSvg({
        width: this.width,
        height: this.height,
        viewBox: `0 0 ${this.width} ${this.height}`,
      });
      this.widget.addControl(this.tControl.element());
      this.widget.setContent(this.canvas.element);
      this.render();
      return this.widget.root;
    }

    xFor(t) {
      return this.plot.x + FigureMath.clamp01(t) * this.plot.width;
    }

    yFor(value) {
      return this.plot.y + (1 - FigureMath.clamp01(value)) * this.plot.height;
    }

    smoothstep(t) {
      return t * t * (3 - 2 * t);
    }

    curvePath() {
      const points = [];
      for (let i = 0; i <= 64; ++i) {
        const t = i / 64;
        points.push(new Vector(this.xFor(t), this.yFor(this.smoothstep(t))));
      }
      return Path.polyline(points);
    }

    renderAxes() {
      const left = this.plot.x;
      const right = this.plot.x + this.plot.width;
      const top = this.plot.y;
      const bottom = this.plot.y + this.plot.height;

      this.canvas.arrow(new Vector(left, bottom), new Vector(right + 26, bottom), {
        stroke: '#222',
        'stroke-width': FigurePixelGuideStrokeWidth,
        markerId: 'interpolation-smoothstep-t-axis',
      });
      this.canvas.arrow(new Vector(left, bottom), new Vector(left, top - 20), {
        stroke: '#222',
        'stroke-width': FigurePixelGuideStrokeWidth,
        markerId: 'interpolation-smoothstep-value-axis',
      });
      this.canvas.text(right + 32, bottom + 4, 't', { 'font-size': 13 });
      this.canvas.text(left - 36, top - 12, 'value', { 'font-size': 13 });

      [0, 0.5, 1].forEach((tick) => {
        const x = this.xFor(tick);
        const y = this.yFor(tick);
        this.canvas.line(new Vector(x, bottom - 5), new Vector(x, bottom + 5), {
          stroke: '#777',
          'stroke-width': FigurePixelGuideStrokeWidth,
        });
        this.canvas.line(new Vector(left - 5, y), new Vector(left + 5, y), {
          stroke: '#777',
          'stroke-width': FigurePixelGuideStrokeWidth,
        });
        this.canvas.text(x - 8, bottom + 24, tick.toFixed(tick === 0.5 ? 1 : 0), {
          'font-size': 12,
          fill: '#555',
        });
      });
    }

    render() {
      this.canvas.clear();
      this.canvas.element.setAttribute('data-interpolator', 'smoothstep');

      this.renderAxes();

      this.canvas.line(new Vector(this.xFor(0), this.yFor(0)), new Vector(this.xFor(1), this.yFor(1)), {
        stroke: '#999',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'stroke-dasharray': '5 4',
      });
      this.canvas.add('path', {
        d: this.curvePath(),
        fill: 'none',
        stroke: '#9c36b5',
        'stroke-width': FigurePixelStrokeWidth,
      });

      const value = this.smoothstep(this.t);
      const sampleX = this.xFor(this.t);
      const sampleY = this.yFor(value);
      this.canvas.line(new Vector(sampleX, this.yFor(0)), new Vector(sampleX, sampleY), {
        stroke: '#999',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'stroke-dasharray': '4 4',
      });
      this.canvas.line(new Vector(this.xFor(0), sampleY), new Vector(sampleX, sampleY), {
        stroke: '#999',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'stroke-dasharray': '4 4',
      });
      this.canvas.add('circle', {
        cx: sampleX,
        cy: sampleY,
        r: 8,
        fill: '#9c36b5',
        stroke: '#111',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
      this.canvas.text(this.xFor(0) - 10, this.yFor(0) - 12, 'A', {
        'font-size': 14,
        'font-weight': 700,
      });
      this.canvas.text(this.xFor(1) + 8, this.yFor(1) + 4, 'B', {
        'font-size': 14,
        'font-weight': 700,
      });
      this.canvas.text(64, 260, `sample = ${value.toFixed(2)}`, {
        'font-size': 13,
        fill: '#333',
      });
    }
  }

  const figure = new InterpolationSmoothStepWidget();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
