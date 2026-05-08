((scriptElement) => {
class SupportMappingGJK {
  constructor() {
    this.separation = 2.1;
    this.step = 3;
  }

  createCanvas() {
    const canvas = new Canvas(520, 360);
    canvas.setTransform('translate(260, 150) scale(42, 42)');

    const shapeA = this.shapeA();
    const shapeB = this.shapeB();
    const iterations = this.gjkIterations(shapeA, shapeB);
    const shown = iterations[Math.min(this.step, iterations.length - 1)];
    const direction = shown.direction.normalized();
    const support = shown.support;
    const simplex = shown.simplex;

    this.drawWorldPanel(canvas, shapeA, shapeB, direction, support);
    this.drawDifferencePanel(canvas, shapeA, shapeB, support, simplex);

    return canvas.toSVG();
  }

  shapeA() {
    return [
      new Vector(-2.9, -0.9),
      new Vector(-1.1, -1.0),
      new Vector(-0.9, 0.7),
      new Vector(-2.6, 1.0)
    ];
  }

  shapeB() {
    const center = new Vector(this.separation - 1.6, 0.1);
    return [
      center.plus(new Vector(-0.7, -0.8)),
      center.plus(new Vector(0.9, -0.2)),
      center.plus(new Vector(-0.1, 0.9))
    ];
  }

  drawWorldPanel(canvas, shapeA, shapeB, direction, support) {
    canvas.add(new Text(new Vector(-5.7, -2.75), 'support functions'));
    canvas.add(new Path(Path.polyline(shapeA, { closed: true }), 'blue'));
    canvas.add(new Path(Path.polyline(shapeB, { closed: true }), 'green'));
    canvas.add(new Text(new Vector(-2.25, 1.45), 'A', 'blue'));
    canvas.add(new Text(new Vector(this.separation - 1.7, 1.45), 'B', 'green'));

    canvas.add(new Line(new Vector(-4.9, 1.8), direction.multiply(1.0), 'arrow red'));
    canvas.add(new Text(new Vector(-4.75, 2.25), 'direction v', 'red'));

    canvas.add(new Line(support.a, direction.multiply(0.7), 'arrow blue'));
    canvas.add(new Line(support.b, direction.multiply(-0.7), 'arrow green'));
    canvas.add(new Circle(support.a, 0.07, 'result'));
    canvas.add(new Circle(support.b, 0.07, 'intersection'));
    canvas.add(new Text(support.a.plus(new Vector(0.13, -0.08)), 'supportA(v)', 'blue'));
    canvas.add(new Text(support.b.plus(new Vector(0.13, 0.18)), 'supportB(-v)', 'green'));
  }

  drawDifferencePanel(canvas, shapeA, shapeB, support, simplex) {
    const offset = new Vector(2.7, 0.2);
    const difference = this.minkowskiDifference(shapeA, shapeB).map(p => p.plus(offset));
    const supportPoint = support.point.plus(offset);
    const origin = offset;

    canvas.add(new Text(new Vector(1.35, -2.75), 'Minkowski difference A - B'));
    canvas.add(new Path(Path.polyline(difference, { closed: true }), 'dashed'));
    canvas.add(new Line(new Vector(offset.x - 2.1, offset.y), new Vector(4.2, 0)));
    canvas.add(new Line(new Vector(offset.x, offset.y - 2.1), new Vector(0, 4.2)));
    canvas.add(new Text(origin.plus(new Vector(0.12, 0.28)), 'origin'));
    canvas.add(new Circle(origin, 0.06, 'intersection'));

    canvas.add(new Circle(supportPoint, 0.08, 'result'));
    canvas.add(new Text(supportPoint.plus(new Vector(0.12, -0.12)), 'A - B support', 'red'));

    const simplexPoints = simplex.map(p => p.plus(offset));
    if (simplexPoints.length > 1) {
      canvas.add(new Path(Path.polyline(simplexPoints, { closed: simplexPoints.length > 2 }), 'red'));
    }
    simplexPoints.forEach((point, i) => {
      canvas.add(new Circle(point, 0.065, 'result'));
      canvas.add(new Text(point.plus(new Vector(0.1, 0.18)), `s${i}`, 'red'));
    });

    if (simplex.length > 0) {
      const closest = this.closestPointToOrigin(simplex).plus(offset);
      canvas.add(new Line(origin, closest.minus(origin), 'arrow red'));
      canvas.add(new Circle(closest, 0.055, 'intersection'));
      canvas.add(new Text(closest.plus(new Vector(0.12, 0.2)), 'closest point'));
    }
  }

  gjkIterations(shapeA, shapeB) {
    let direction = new Vector(1, -0.15);
    let simplex = [];
    const iterations = [];

    for (let i = 0; i < 5; i++) {
      const support = this.supportDifference(shapeA, shapeB, direction);
      simplex.unshift(support.point);
      iterations.push({
        direction,
        support,
        simplex: simplex.slice()
      });

      const next = this.nextDirection(simplex);
      simplex = next.simplex;
      if (next.containsOrigin) break;
      direction = next.direction;
    }

    return iterations;
  }

  nextDirection(simplex) {
    if (simplex.length === 1) {
      return { simplex, direction: simplex[0].multiply(-1), containsOrigin: false };
    }

    const a = simplex[0];
    const b = simplex[1];
    const ao = a.multiply(-1);
    const ab = b.minus(a);

    if (simplex.length === 2) {
      if (ab.dot(ao) > 0) {
        return {
          simplex,
          direction: this.tripleProduct(ab, ao, ab),
          containsOrigin: false
        };
      }
      return { simplex: [a], direction: ao, containsOrigin: false };
    }

    const c = simplex[2];
    const ac = c.minus(a);
    const abPerp = this.tripleProduct(ac, ab, ab);
    const acPerp = this.tripleProduct(ab, ac, ac);

    if (abPerp.dot(ao) > 0) {
      return { simplex: [a, b], direction: abPerp, containsOrigin: false };
    }
    if (acPerp.dot(ao) > 0) {
      return { simplex: [a, c], direction: acPerp, containsOrigin: false };
    }
    return { simplex, direction: Vector.null, containsOrigin: true };
  }

  supportDifference(shapeA, shapeB, direction) {
    const a = this.supportPoint(shapeA, direction);
    const b = this.supportPoint(shapeB, direction.multiply(-1));
    return { a, b, point: a.minus(b) };
  }

  supportPoint(points, direction) {
    return points.reduce((best, point) =>
      point.dot(direction) > best.dot(direction) ? point : best
    );
  }

  minkowskiDifference(shapeA, shapeB) {
    const points = [];
    shapeA.forEach(a => shapeB.forEach(b => points.push(a.minus(b))));
    return this.convexHull(points);
  }

  convexHull(points) {
    const sorted = points.slice().sort((a, b) => a.x === b.x ? a.y - b.y : a.x - b.x);
    const cross = (o, a, b) => a.minus(o).x * b.minus(o).y - a.minus(o).y * b.minus(o).x;
    const lower = [];
    sorted.forEach((point) => {
      while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], point) <= 0) {
        lower.pop();
      }
      lower.push(point);
    });
    const upper = [];
    sorted.slice().reverse().forEach((point) => {
      while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], point) <= 0) {
        upper.pop();
      }
      upper.push(point);
    });
    lower.pop();
    upper.pop();
    return lower.concat(upper);
  }

  closestPointToOrigin(simplex) {
    if (simplex.length === 1) return simplex[0];
    if (simplex.length === 2) return this.closestPointOnSegment(Vector.null, simplex[0], simplex[1]);
    return Vector.null;
  }

  closestPointOnSegment(point, a, b) {
    const ab = b.minus(a);
    const t = Math.max(0, Math.min(1, point.minus(a).dot(ab) / ab.dot(ab)));
    return a.plus(ab.multiply(t));
  }

  tripleProduct(a, b, c) {
    const ac = a.dot(c);
    const bc = b.dot(c);
    const result = b.multiply(ac).minus(a.multiply(bc));
    return result.length() < 1e-9 ? new Vector(-c.y, c.x) : result;
  }
}

  const figure = new SupportMappingGJK();
  const container = document.createElement('div');

  let canvas = figure.createCanvas();
  const redraw = () => {
    const newCanvas = figure.createCanvas();
    container.replaceChild(newCanvas, canvas);
    canvas = newCanvas;
  };

  container.appendChild(new Slider({
    label: 'shape separation',
    min: 1.4,
    max: 2.8,
    step: 0.1,
    value: figure.separation,
    onChange: (value) => {
      figure.separation = value;
      redraw();
    }
  }).element());

  container.appendChild(new Slider({
    label: 'GJK step',
    min: 0,
    max: 4,
    step: 1,
    value: figure.step,
    precision: 0,
    onChange: (value) => {
      figure.step = Math.round(value);
      redraw();
    }
  }).element());

  container.appendChild(canvas);
  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
