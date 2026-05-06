class SphereFarthestPoint {
  constructor() {
    this.radius = 2;
    this.angle = 12 * degrees;
  }

  createCanvas() {
    const canvas = new Canvas(320, 240);
    canvas.center();

    const direction = Vector.up.rotated(this.angle);

    // plot direction vector
    canvas.add(new Line(Vector.null, direction, 'arrow'));

    // plot the sphere (cross-section circle)
    canvas.add(new Circle(Vector.null, this.radius));

    // plot the resulting point
    const point = this.farthestPoint(direction);
    canvas.add(new Circle(point, 0.05, 'result'));

    // plot the tangential line — perpendicular to direction at the
    // farthest-point.
    const tangential = direction.rotated(90 * degrees);
    canvas.add(new Line(point.plus(tangential.multiply(-50)),
                        tangential.multiply(100), 'dashed'));

    return canvas.toSVG();
  }

  farthestPoint(direction) {
    return direction.multiply(this.radius);
  }
}

((scriptElement) => {
  const figure = new SphereFarthestPoint();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.angle += delta.x * degrees;
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
