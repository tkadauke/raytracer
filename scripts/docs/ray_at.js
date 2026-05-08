const clampRay = (value, min, max) => Math.max(min, Math.min(max, value));

class RayAt {
  constructor() {
    this.origin = new Vector(4, -1);
    this.angle = 12 * degrees;
    this.length = 3;
    this.t = 0.5;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'ray-at-widget' });
    this.tControl = new FigureSliderControl({
      label: 't',
      min: -1.0,
      max: 1.7,
      step: 0.01,
      value: this.t,
      precision: 2,
      onChange: (value) => {
        this.setT(value);
        this.render();
      },
    });
    this.widget.addControl(this.tControl.element());
    this.render();
    return this.widget.root;
  }

  setT(t) {
    this.t = clampRay(t, -1.0, 1.7);
  }

  createCanvas() {
    const canvas = new Canvas(320, 240);
    canvas.translate(new Vector(2, -2));

    canvas.add(new Axes());

    const direction = Vector.up.rotated(this.angle).multiply(this.length);

    // plot origin vector
    canvas.add(new Line(Vector.null, this.origin, 'arrow'));

    // plot origin point
    canvas.add(new Circle(this.origin, 0.05, 'intersection'));

    const ray = new Ray(this.origin, direction);

    // plot the ray
    canvas.add(ray);
    canvas.add(new Line(this.origin, direction, 'arrow'));

    const point = ray.at(this.t);
    canvas.add(new Circle(point, 0.14, 'result'));
    canvas.add(new Text(point.plus(new Vector(0.4, 0.2)), `t=${this.t.toFixed(2)}`));

    return canvas.toSVG();
  }

  render() {
    this.widget.setContent(this.createCanvas());
  }
}

((scriptElement) => {
  const figure = new RayAt();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
