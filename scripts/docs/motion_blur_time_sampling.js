// Interactive widget for motion blur's shutter-time sampling.
// The static primitive is evaluated at several time offsets during
// one shutter interval; linear velocity translates each intersection
// test to a different position and the averaged samples produce the
// ghosted silhouette.

class MotionBlurTimeSampling {
  constructor() {
    this.width = 560;
    this.height = 260;
    this.center = { x: 150, y: 132 };
    this.velocity = { x: 260, y: -32 };
    this.radius = 34;
    this.sampleCount = 8;
    this.shutterTime = 0.5;
    this.mode = 'stochastic';
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'motion-blur-time-sampling-widget' });
    this.timeControl = new FigureSliderControl({
      label: 'shutter time',
      min: 0,
      max: 1,
      step: 0.01,
      value: this.shutterTime,
      precision: 2,
      onChange: (value) => {
        this.shutterTime = value;
        this.render();
      },
    });
    this.modeControl = new FigureSegmentedControl({
      label: 'sampling',
      value: this.mode,
      options: [
        { label: 'regular', value: 'regular' },
        { label: 'stochastic', value: 'stochastic' },
      ],
      onChange: (value) => {
        this.mode = value;
        this.render();
      },
    });

    this.widget.addControl(this.timeControl.element());
    this.widget.addControl(this.modeControl.element());
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

  endpoint() {
    return {
      x: this.center.x + this.velocity.x,
      y: this.center.y + this.velocity.y,
    };
  }

  setEndpoint(point) {
    this.velocity = {
      x: this.clamp(point.x - this.center.x, -80, 340),
      y: this.clamp(point.y - this.center.y, -96, 96),
    };
  }

  positionAt(t) {
    return {
      x: this.center.x + this.velocity.x * t,
      y: this.center.y + this.velocity.y * t,
    };
  }

  sampleTimes() {
    if (this.mode === 'regular') {
      // RegularSampler has one value in its 1D shutter dimension, so every
      // pixel sample sees the same half-shutter time.
      return Array(this.sampleCount).fill(0.5);
    }

    // Deterministic jittered pattern: stable for tests and docs, but visibly
    // stratified across the shutter interval.
    const jitter = [0.17, 0.73, 0.31, 0.92, 0.48, 0.08, 0.64, 0.39];
    return jitter.map((offset, index) => (index + offset) / this.sampleCount);
  }

  render() {
    this.canvas.clear();
    this.renderGhosts();
    this.renderPath();
    this.renderSamples();
    this.renderCurrentTime();
    this.renderVelocityHandle();
    this.renderLabels();
  }

  renderGhosts() {
    const times = this.sampleTimes();
    times.forEach((t, index) => {
      const p = this.positionAt(t);
      this.canvas.add('circle', {
        cx: p.x,
        cy: p.y,
        r: this.radius,
        fill: '#2b8a3e',
        'fill-opacity': this.mode === 'regular' ? 0.18 : 0.085,
        stroke: '#1b5e2d',
        'stroke-opacity': this.mode === 'regular' ? 0.45 : 0.25,
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-ghost-silhouette': '1',
        'data-sample-index': index,
        'data-sample-time': t.toFixed(3),
      });
    });
  }

  renderPath() {
    const start = this.positionAt(0);
    const end = this.positionAt(1);
    this.canvas.add('line', {
      x1: start.x,
      y1: start.y,
      x2: end.x,
      y2: end.y,
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      'stroke-linecap': 'round',
      'marker-end': 'url(#motion-blur-arrow)',
      'data-object-path': '1',
    });

    this.canvas.add('defs').innerHTML = `
      <marker id="motion-blur-arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#111"></path>
      </marker>
    `;
  }

  renderSamples() {
    this.sampleTimes().forEach((t) => {
      const p = this.positionAt(t);
      this.canvas.add('circle', {
        cx: p.x,
        cy: p.y,
        r: 5,
        fill: '#0b7285',
        stroke: '#083f4a',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-time-sample': '1',
        'data-sample-time': t.toFixed(3),
      });
    });
  }

  renderCurrentTime() {
    const p = this.positionAt(this.shutterTime);
    this.canvas.add('circle', {
      cx: p.x,
      cy: p.y,
      r: this.radius,
      fill: 'none',
      stroke: '#c92a2a',
      'stroke-width': FigurePixelStrokeWidth,
      'data-current-shutter-time': this.shutterTime.toFixed(2),
    });
    this.canvas.add('line', {
      x1: p.x,
      y1: 28,
      x2: p.x,
      y2: this.height - 34,
      stroke: '#c92a2a',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '4 5',
      'data-current-time-guide': '1',
    });
  }

  renderVelocityHandle() {
    const end = this.endpoint();
    this.canvas.add('line', {
      x1: this.center.x,
      y1: this.center.y,
      x2: end.x,
      y2: end.y,
      stroke: '#364fc7',
      'stroke-width': FigurePixelStrokeWidth,
      'stroke-linecap': 'round',
      'data-velocity-vector': '1',
    });

    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: end,
      radius: 9,
      attrs: {
        fill: '#4263eb',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'velocity-end',
      },
      onDrag: (point) => {
        this.setEndpoint(point);
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }

  renderLabels() {
    const start = this.positionAt(0);
    const end = this.positionAt(1);
    const current = this.positionAt(this.shutterTime);
    const label = (text, x, y, attrs = {}) => {
      const el = this.canvas.add('text', {
        x,
        y,
        'font-family': 'sans-serif',
        'font-size': 13,
        fill: '#222',
        ...attrs,
      });
      el.textContent = text;
      return el;
    };

    label('t = 0', start.x - 18, this.height - 15);
    label('t = 1', end.x - 16, this.height - 15);
    label(`current t = ${this.shutterTime.toFixed(2)}`, current.x - 42, 20, {
      fill: '#c92a2a',
      'data-current-time-label': '1',
    });
    label('sampled positions', 18, 28);
    label('accumulated silhouettes', 18, 48);
  }
}

((scriptElement) => {
  const figure = new MotionBlurTimeSampling();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
