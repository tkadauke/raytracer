((scriptElement) => {
  class InterpolationStepWidget {
    constructor() {
      this.t = 0.35;
      this.width = 560;
      this.height = 290;
      this.plot = { x: 64, y: 34, width: 420, height: 190 };
    }

    element() {
      if (this.widget) return this.widget.root;

      this.widget = new FigureWidget({ className: 'interpolation-step-widget' });
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

    sample() {
      return this.t >= 1 ? 1 : 0;
    }

    renderAxes() {
      const left = this.plot.x;
      const right = this.plot.x + this.plot.width;
      const top = this.plot.y;
      const bottom = this.plot.y + this.plot.height;

      this.canvas.arrow(new Vector(left, bottom), new Vector(right + 26, bottom), {
        stroke: '#222',
        'stroke-width': FigurePixelGuideStrokeWidth,
        markerId: 'interpolation-step-t-axis',
      });
      this.canvas.arrow(new Vector(left, bottom), new Vector(left, top - 20), {
        stroke: '#222',
        'stroke-width': FigurePixelGuideStrokeWidth,
        markerId: 'interpolation-step-value-axis',
      });
      this.canvas.text(right + 32, bottom + 4, 't', { 'font-size': 13 });
      this.canvas.text(left - 36, top - 12, 'value', { 'font-size': 13 });

      [0, 0.5, 1].forEach((tick) => {
        const x = this.xFor(tick);
        this.canvas.line(new Vector(x, bottom - 5), new Vector(x, bottom + 5), {
          stroke: '#777',
          'stroke-width': FigurePixelGuideStrokeWidth,
        });
        this.canvas.text(x - 8, bottom + 24, tick.toFixed(tick === 0.5 ? 1 : 0), {
          'font-size': 12,
          fill: '#555',
        });
      });
      [0, 1].forEach((tick) => {
        const y = this.yFor(tick);
        this.canvas.line(new Vector(left - 5, y), new Vector(left + 5, y), {
          stroke: '#777',
          'stroke-width': FigurePixelGuideStrokeWidth,
        });
        this.canvas.text(left - 24, y + 4, `${tick}`, {
          'font-size': 12,
          fill: '#555',
        });
      });
    }

    render() {
      this.canvas.clear();
      this.canvas.element.setAttribute('data-interpolator', 'step');

      this.renderAxes();

      const y0 = this.yFor(0);
      const y1 = this.yFor(1);
      const x0 = this.xFor(0);
      const x1 = this.xFor(1);
      this.canvas.line(new Vector(x0, y0), new Vector(x1, y0), {
        stroke: '#1c7ed6',
        'stroke-width': FigurePixelStrokeWidth,
      });
      this.canvas.line(new Vector(x1, y0), new Vector(x1, y1), {
        stroke: '#1c7ed6',
        'stroke-width': FigurePixelStrokeWidth,
        'stroke-dasharray': '5 4',
      });

      const sampleX = this.xFor(this.t);
      const sampleY = this.yFor(this.sample());
      this.canvas.line(new Vector(sampleX, this.yFor(0)), new Vector(sampleX, this.yFor(1)), {
        stroke: '#999',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'stroke-dasharray': '4 4',
      });
      this.canvas.add('circle', {
        cx: sampleX,
        cy: sampleY,
        r: 8,
        fill: '#1c7ed6',
        stroke: '#111',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
      this.canvas.text(x0 - 10, y0 - 12, 'A', { 'font-size': 14, 'font-weight': 700 });
      this.canvas.text(x1 + 8, y1 + 4, 'B', { 'font-size': 14, 'font-weight': 700 });
      this.canvas.text(64, 260, `sample = ${this.sample() >= 1 ? 'B' : 'A'}`, {
        'font-size': 13,
        fill: '#333',
      });
    }
  }

  const figure = new InterpolationStepWidget();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
