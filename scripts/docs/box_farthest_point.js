class BoxFarthestPoint {
  constructor() {
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.angleDegrees = 12;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'box-farthest-point-widget' });
    this.angleControl = new FigureSliderControl({
      label: 'direction angle',
      min: 0,
      max: 360,
      step: 1,
      value: this.angleDegrees,
      precision: 0,
      onChange: (value) => {
        this.angleDegrees = value;
        this.render();
      },
    });
    this.widget.addControl(this.angleControl.element());
    this.render();
    return this.widget.root;
  }

  angle() {
    return this.angleDegrees * degrees;
  }

  createCanvas() {
    const canvas = new Canvas(320, 240);
    canvas.center();

    const direction = Vector.up.rotated(this.angle());

    // plot direction vector
    canvas.add(new Line(Vector.null, direction, 'arrow'));

    // plot the box
    canvas.add(new Rectangle(this.topleft, this.size));

    // plot the resulting point
    const point = this.farthestPoint(direction);
    canvas.add(new Circle(point, 0.14, 'result'));

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

  render() {
    this.widget.setContent(this.createCanvas());
  }
}

((scriptElement) => {
  const figure = new BoxFarthestPoint();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
