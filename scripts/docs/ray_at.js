const clampRay = (value, min, max) => Math.max(min, Math.min(max, value));

class RayAt {
  constructor() {
    this.origin = new Vector(4, -1);
    this.angle = 12 * degrees;
    this.length = 3;
    this.t = 0.5;
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
    canvas.add(new Circle(point, 0.05, 'result'));
    canvas.add(new Text(point.plus(new Vector(0.4, 0.2)), `t=${this.t.toFixed(2)}`));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new RayAt();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.setT(figure.t + delta.x / 100.0);
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
