class ConvexHullFarthestPoint {
  constructor() {
    this.angle = 162 * degrees;
  }

  createCanvas() {
    const direction = Vector.up.rotated(this.angle);

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
    canvas.add(new Circle(point, 0.05, 'result'));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new ConvexHullFarthestPoint();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.angle += delta.x * degrees;
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
