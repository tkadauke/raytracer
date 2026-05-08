class ConvexHullFarthestPoint {
  constructor() {
    this.angleDegrees = 162;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'convex-hull-farthest-point-widget' });
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
    const direction = Vector.up.rotated(this.angle());

    const canvas = new Canvas(320, 240);
    canvas.translate(new Vector(2, -1));

    canvas.add(new Axes());

    // the line onto which we project the farthest points of all the
    // circles in the hull
    const projection = new Ray(Vector.null, direction, true);

    // plot the ray
    canvas.add(projection);

    // ordered hash holds all the farthest points keyed by projected
    // distance — the maximum-key entry is the convex-hull farthest
    // point along `direction`.
    const distances = new OrderedHash();

    [
      new Vector(2, -3),
      new Vector(6, -4),
      new Vector(4, -5)
    ].forEach((center) => {
      canvas.add(new Circle(center, 1));
      canvas.add(new Line(center, direction, 'arrow'));

      const farthest = center.plus(direction);
      canvas.add(new Circle(farthest, 0.05, 'intersection'));

      const distance = projection.projectedDistance(farthest);
      distances.push(distance, farthest);

      const projected = projection.at(distance).minus(farthest);
      canvas.add(new Line(farthest, projected, 'dashed'));
      canvas.add(new Circle(projection.at(distance), 0.05, 'intersection'));
    });

    // calculate the farthest point — the one with the largest
    // projected distance along `direction`.
    const keys = distances.sortedKeys();
    const point = distances.get(keys[keys.length - 1]);
    canvas.add(new Circle(point, 0.14, 'result'));

    return canvas.toSVG();
  }

  render() {
    this.widget.setContent(this.createCanvas());
  }
}

((scriptElement) => {
  const figure = new ConvexHullFarthestPoint();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
