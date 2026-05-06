class HitpointClass {
  constructor() {
    this.center = new Vector(5, -4);
    this.radius = 1;
  }

  createCanvas() {
    const canvas = new Canvas(320, 240);
    canvas.translate(new Vector(2, -2));

    // plot the point of reference
    canvas.add(new Axes());
    canvas.add(new Text(new Vector(-0.4, 0.4), 'r'));

    // plot the circle
    canvas.add(new Circle(this.center, 1));

    // plot the ray
    const hitnormal = new Vector(0, this.radius);
    const hitpoint = this.center.plus(hitnormal);
    const direction = hitpoint.normalized();
    canvas.add(new Ray(Vector.null, direction));
    canvas.add(new Text(hitpoint.multiply(0.5).plus(new Vector(-0.2, -0.2)), 'd'));

    // plot the intersection point
    canvas.add(new Circle(hitpoint, 0.05));
    canvas.add(new Text(hitpoint.plus(new Vector(-0.2, -0.4)), 'p'));

    // plot the normal at the intersection point
    canvas.add(new Line(hitpoint, hitnormal, 'arrow'));
    canvas.add(new Text(hitpoint.plus(new Vector(0.2, 0.6)), 'n'));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new HitpointClass();
  scriptElement.parentNode.appendChild(figure.createCanvas());
})(document.currentScript);
