class RayProject {
  constructor() {
    this.angle = 34 * degrees;
    this.numPoints = 8;
    this.origin = Vector.null;
  }

  createCanvas() {
    const direction = Vector.up.rotated(this.angle);

    const canvas = new Canvas(320, 240);
    canvas.center();

    canvas.add(new Axes());

    // the line onto which we project the farthest points of all the
    // circles
    const projection = new Ray(this.origin, direction, true);

    // plot the ray
    canvas.add(projection);

    for (let i = 0; i !== this.numPoints; ++i) {
      const center = new Vector(Math.random() * 10 - 5.5,
                                -Math.random() * 8 + 4);
      canvas.add(new Circle(center, 0.05, 'intersection'));

      const projected = projection.projected(center).minus(center);
      canvas.add(new Line(center, projected, 'dashed'));
      canvas.add(new Circle(projected.plus(center), 0.05, 'intersection'));
    }

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new RayProject();
  scriptElement.parentNode.appendChild(figure.createCanvas());
})(document.currentScript);
