// Interactive widget for Tonemap.h's class docstring — plots all
// three operators' transfer functions on the same axes so the
// reader can see the *shape* of "Linear clamps abruptly,"
// "Reinhard compresses smoothly," and "ACES has punchy midtones"
// instead of just inferring it from the rendered comparison
// images above.
//
// Plot:
//   x = input HDR luminance, in [0, 5]
//   y = output LDR (post-tonemap, post-clamp), in [0, 1]
//
//   - Linear (red, dashed):   y = clamp(x, 0, 1)
//   - Reinhard (blue):        y = x / (1 + x)
//   - ACES (green):           y = clamp((x*(2.51x + 0.03)) / (x*(2.43x + 0.59) + 0.14))
//
// A draggable slider sets the "current input" — a vertical line
// drops from x to each curve, and dots mark where each operator
// lands. The output values appear beside each dot.

class TonemapCurves {
  constructor() {
    this.input = 1.0;          // current input HDR value
    this.maxInput = 5.0;       // domain of the plot
    this.plotWidth = 6;        // canvas units for x range
    this.plotHeight = 4;       // canvas units for y range (output 0..1 mapped to 0..plotHeight)
    this.sampleCount = 80;
  }

  setInput(value) {
    this.input = Math.max(0.0, Math.min(this.maxInput, value));
  }

  // Each operator as a pure function. Mirrors the C++ implementations
  // in include/raytracer/tonemap/{Linear,Reinhard,Aces}Tonemap.h —
  // any change to those should be mirrored here.
  linearOf(x) {
    return Math.max(0, Math.min(1, x));
  }

  reinhardOf(x) {
    return x / (1.0 + x);
  }

  acesOf(x) {
    const a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    const y = (x * (a * x + b)) / (x * (c * x + d) + e);
    return Math.max(0, Math.min(1, y));
  }

  // Map a (input, output) data point into canvas-local coordinates.
  // Canvas y is inverted (down is positive), so we negate.
  toCanvas(x, y) {
    const xUnits = (x / this.maxInput) * this.plotWidth;
    const yUnits = -y * this.plotHeight;
    return new Vector(xUnits, yUnits);
  }

  // Sample a function over the plot domain and produce a Path
  // d-string suitable for drawing as a polyline.
  curvePath(fn) {
    const points = [];
    for (let i = 0; i <= this.sampleCount; i++) {
      const x = (i / this.sampleCount) * this.maxInput;
      points.push(this.toCanvas(x, fn(x)));
    }
    return Path.polyline(points);
  }

  createCanvas() {
    const canvas = new Canvas(420, 240);
    // Canvas: 240px / scale-30 = 8 scene units tall. The plot needs
    // ~4.6 units above the origin (plotHeight=4 + arrow extension)
    // and ~0.5 units below for the cursor label. Translate origin up
    // 2.5 units → 5.5 above and 2.5 below, comfortable margins on
    // both sides. (An earlier translate(_, -5) put origin too high
    // and clipped the upper third of every curve.)
    canvas.translate(new Vector(0.7, -2.5));

    // Axes — `Axes` draws +x rightward and +y upward at the right
    // length, but defaults to length 3. We want axes that span the
    // plot bounds, so build them manually.
    canvas.add(new Line(new Vector(0, 0),
                        new Vector(this.plotWidth + 0.6, 0),
                        'axis'));
    canvas.add(new Line(new Vector(0, 0),
                        new Vector(0, -this.plotHeight - 0.6),
                        'axis'));
    canvas.add(new Text(new Vector(this.plotWidth + 0.8, 0.4), 'input (HDR)'));
    canvas.add(new Text(new Vector(-0.6, -this.plotHeight - 0.7), 'output'));

    // Grid lines at output = 1 (the LDR ceiling) and at input = 1
    // (the saturation point). The Reinhard/ACES curves should sit
    // BELOW the y=1 line at all times; Linear hits it and stays.
    canvas.add(new Line(new Vector(0, -this.plotHeight),
                        new Vector(this.plotWidth, 0),
                        'dashed'));
    canvas.add(new Text(new Vector(this.plotWidth + 0.25, -this.plotHeight + 0.1), '1.0'));
    const inputOnePos = (1.0 / this.maxInput) * this.plotWidth;
    canvas.add(new Line(new Vector(inputOnePos, 0),
                        new Vector(0, -this.plotHeight),
                        'dashed'));

    // The three curves.
    canvas.add(new Path(this.curvePath((x) => this.linearOf(x)),   'red dashed'));
    canvas.add(new Path(this.curvePath((x) => this.reinhardOf(x)), 'blue'));
    canvas.add(new Path(this.curvePath((x) => this.acesOf(x)),     'green'));

    // Legend — short labeled segments at fixed positions. Order
    // (Linear/Reinhard/ACES) matches the C++ class declaration
    // order and the comparison-image table in Tonemap.h.
    const legendX = this.plotWidth + 1.0;
    const legendY = -this.plotHeight + 0.4;
    canvas.add(new Line(new Vector(legendX, legendY),
                        new Vector(0.4, 0),
                        'red dashed'));
    canvas.add(new Text(new Vector(legendX + 0.5, legendY + 0.1), 'Linear'));
    canvas.add(new Line(new Vector(legendX, legendY + 0.4),
                        new Vector(0.4, 0),
                        'blue'));
    canvas.add(new Text(new Vector(legendX + 0.5, legendY + 0.5), 'Reinhard'));
    canvas.add(new Line(new Vector(legendX, legendY + 0.8),
                        new Vector(0.4, 0),
                        'green'));
    canvas.add(new Text(new Vector(legendX + 0.5, legendY + 0.9), 'ACES'));

    // Vertical "current input" indicator + dots on each curve at
    // that x.
    const x = this.input;
    const cursorPos = (x / this.maxInput) * this.plotWidth;
    canvas.add(new Line(new Vector(cursorPos, 0),
                        new Vector(0, -this.plotHeight),
                        'dashed'));

    const yL = this.linearOf(x);
    const yR = this.reinhardOf(x);
    const yA = this.acesOf(x);

    canvas.add(new Circle(this.toCanvas(x, yL), 0.08, 'result'));
    canvas.add(new Circle(this.toCanvas(x, yR), 0.08, 'intersection'));
    canvas.add(new Circle(this.toCanvas(x, yA), 0.08, 'intersection'));

    // Numeric readout under the cursor x.
    canvas.add(new Text(new Vector(cursorPos - 0.4, 0.45),
                        `x=${x.toFixed(2)}`));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new TonemapCurves();

  const container = document.createElement('div');
  let canvas = figure.createCanvas();
  container.appendChild(canvas);

  const slider = new Slider({
    label: 'input HDR luminance',
    min: 0,
    max: 5.0,
    value: figure.input,
    step: 0.05,
    precision: 2,
    onChange: (v) => {
      figure.setInput(v);
      const newCanvas = figure.createCanvas();
      container.replaceChild(newCanvas, canvas);
      canvas = newCanvas;
    }
  });
  container.appendChild(slider.element());

  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
