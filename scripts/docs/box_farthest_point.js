class BoxFarthestPoint {
  constructor() {
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.angle = 12 * degrees;
  }

  createCanvas() {
    const canvas = new Canvas(320, 240);
    canvas.center();

    const direction = Vector.up.rotated(this.angle);

    // plot direction vector
    canvas.add(new Line(Vector.null, direction, 'arrow'));

    // plot the box
    canvas.add(new Rectangle(this.topleft, this.size));

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
    return this.topleft.plus(new Vector(
      direction.x < 0 ? 0 : this.size.x,
      direction.y < 0 ? 0 : this.size.y
    ));
  }
}

((scriptElement) => {
  const figure = new BoxFarthestPoint();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.angle += delta.x * degrees;
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
