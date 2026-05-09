// Interactive widget for support mapping and GJK.
//
// The widget intentionally keeps the story to two linked panels:
// first choose support points on A and B, then add their difference
// to the simplex that GJK moves toward the origin.

class SupportMappingGJK {
  constructor() {
    this.width = 640;
    this.height = 300;
    this.separation = 2.1;
    this.step = 2;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'support-mapping-gjk-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.separationControl = new FigureSliderControl({
      label: 'shape separation',
      min: 1.4,
      max: 2.8,
      step: 0.1,
      value: this.separation,
      precision: 2,
      onChange: (value) => {
        this.separation = value;
        this.render();
      },
    });

    this.stepControl = new FigureSliderControl({
      label: 'GJK step',
      min: 0,
      max: 4,
      step: 1,
      value: this.step,
      precision: 0,
      onChange: (value) => {
        this.step = Math.round(value);
        this.render();
      },
    });

    this.widget.addControl(this.separationControl.element());
    this.widget.addControl(this.stepControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  supportPanel() {
    return {
      x: 24,
      y: 54,
      width: 286,
      height: 210,
      origin: { x: 158, y: 160 },
      scale: 48,
    };
  }

  differencePanel() {
    return {
      x: 338,
      y: 54,
      width: 278,
      height: 210,
      origin: { x: 528, y: 160 },
      scale: 38,
    };
  }

  shapeA() {
    return [
      new Vector(-2.35, -0.62),
      new Vector(-1.04, -0.76),
      new Vector(-0.86, 0.62),
      new Vector(-2.16, 0.78),
    ];
  }

  shapeB() {
    const center = new Vector(this.separation - 1.05, 0.0);
    return [
      center.plus(new Vector(-0.62, -0.78)),
      center.plus(new Vector(0.82, -0.18)),
      center.plus(new Vector(-0.12, 0.88)),
    ];
  }

  render() {
    this.canvas.clear();
    this.addArrowDefs();

    const shapeA = this.shapeA();
    const shapeB = this.shapeB();
    const steps = this.supportSteps(shapeA, shapeB);
    const index = Math.min(this.step, steps.length - 1);
    const shown = steps[index];

    this.renderSupportPanel(shapeA, shapeB, shown, index);
    this.renderDifferencePanel(shapeA, shapeB, shown, index);
    this.renderExplanation(shown, index);
  }

  addArrowDefs() {
    const defs = this.canvas.add('defs');
    defs.innerHTML = `
      <marker id="gjk-arrow-gray" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#333"></path>
      </marker>
      <marker id="gjk-arrow-blue" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#2060d0"></path>
      </marker>
      <marker id="gjk-arrow-green" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#20a050"></path>
      </marker>
      <marker id="gjk-arrow-red" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#e03131"></path>
      </marker>
    `;
  }

  map(panel, point) {
    return {
      x: panel.origin.x + point.x * panel.scale,
      y: panel.origin.y - point.y * panel.scale,
    };
  }

  addText(x, y, text, attrs = {}) {
    const element = this.canvas.add('text', {
      x,
      y,
      fill: '#222',
      'font-family': '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
      'font-size': 13,
      ...attrs,
    });
    element.textContent = text;
    return element;
  }

  addPanel(panel, title, attrs = {}) {
    this.canvas.add('rect', {
      x: panel.x,
      y: panel.y,
      width: panel.width,
      height: panel.height,
      rx: 6,
      fill: '#fbfbfb',
      stroke: '#dddddd',
      'stroke-width': FigurePixelGuideStrokeWidth,
      ...attrs,
    });
    this.addText(panel.x + 14, panel.y + 26, title, {
      'font-size': 17,
      'font-weight': 700,
    });
  }

  addPath(points, attrs = {}) {
    const closed = attrs.closed !== false;
    const pathAttrs = { ...attrs };
    delete pathAttrs.closed;
    const d = points.map((point, index) =>
      `${index === 0 ? 'M' : 'L'} ${point.x} ${point.y}`).join(' ');
    return this.canvas.add('path', {
      d: closed ? `${d} Z` : d,
      fill: 'none',
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
      ...pathAttrs,
    });
  }

  addLine(from, to, attrs = {}) {
    return this.canvas.add('line', {
      x1: from.x,
      y1: from.y,
      x2: to.x,
      y2: to.y,
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
      ...attrs,
    });
  }

  addCircle(point, radius, attrs = {}) {
    return this.canvas.add('circle', {
      cx: point.x,
      cy: point.y,
      r: radius,
      fill: '#ffffff',
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
      ...attrs,
    });
  }

  renderSupportPanel(shapeA, shapeB, shown) {
    const panel = this.supportPanel();
    const direction = shown.direction.normalized();
    const support = shown.support;
    this.addPanel(panel, '1. Ask support functions', { 'data-gjk-panel': 'support' });

    this.addPath(shapeA.map(point => this.map(panel, point)), {
      fill: '#e8f0ff',
      stroke: '#2060d0',
      'data-gjk-shape': 'A',
    });
    this.addPath(shapeB.map(point => this.map(panel, point)), {
      fill: '#eaf8ef',
      stroke: '#20a050',
      'data-gjk-shape': 'B',
    });

    this.addText(this.map(panel, new Vector(-1.65, -1.08)).x,
      this.map(panel, new Vector(-1.65, -1.08)).y, 'A', {
        fill: '#2060d0',
        'font-size': 15,
        'font-weight': 700,
      });
    this.addText(this.map(panel, new Vector(this.separation - 1.12, -1.08)).x,
      this.map(panel, new Vector(this.separation - 1.12, -1.08)).y, 'B', {
        fill: '#20a050',
        'font-size': 15,
        'font-weight': 700,
        'text-anchor': 'middle',
      });

    this.renderDirectionInset(panel, direction);
    this.renderSupportPoint(panel, support.a, direction, '#2060d0',
      'supportA(v)', 'A');
    this.renderSupportPoint(panel, support.b, direction.multiply(-1), '#20a050',
      'supportB(-v)', 'B');
  }

  renderDirectionInset(panel, direction) {
    const start = { x: panel.x + 24, y: panel.y + panel.height - 30 };
    const end = {
      x: start.x + direction.x * 52,
      y: start.y - direction.y * 52,
    };
    this.addLine(start, end, {
      stroke: '#333',
      'marker-end': 'url(#gjk-arrow-gray)',
      'data-gjk-direction': 'v',
    });
    this.addText(start.x, start.y + 20, 'direction v', {
      'font-size': 12,
      fill: '#555',
    });
  }

  renderSupportPoint(panel, point, direction, color, label, kind) {
    const screen = this.map(panel, point);
    const arrowStart = this.map(panel, point.minus(direction.normalized().multiply(0.52)));
    const arrowEnd = this.map(panel, point.plus(direction.normalized().multiply(0.35)));

    this.addLine(arrowStart, arrowEnd, {
      stroke: color,
      'marker-end': kind === 'A' ? 'url(#gjk-arrow-blue)' : 'url(#gjk-arrow-green)',
      'data-gjk-support-direction': kind,
    });
    this.addCircle(screen, 5, {
      fill: color,
      stroke: '#ffffff',
      'data-gjk-support-point': kind,
    });

    const labelPoint = kind === 'A'
      ? { x: panel.x + 176, y: panel.y + 56 }
      : { x: panel.x + 176, y: panel.y + panel.height - 36 };
    this.addLine(labelPoint, screen, {
      stroke: color,
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '4 4',
    });
    this.addText(labelPoint.x, labelPoint.y, label, {
      fill: color,
      'font-size': 12,
      'font-weight': 700,
      'data-gjk-callout': label,
    });
  }

  renderDifferencePanel(shapeA, shapeB, shown, index) {
    const panel = this.differencePanel();
    const support = shown.support;
    const simplex = shown.simplex;
    const difference = this.minkowskiDifference(shapeA, shapeB);
    const supportPoint = support.point;
    const closest = this.closestPointToOrigin(simplex);
    this.addPanel(panel, '2. Add A - B point', { 'data-gjk-panel': 'minkowski' });

    this.renderAxes(panel);
    this.addPath(difference.map(point => this.map(panel, point)), {
      fill: '#fff1f1',
      stroke: '#e03131',
      'stroke-dasharray': '5 5',
      'data-gjk-difference': 'A-minus-B',
    });

    if (simplex.length > 1) {
      this.addPath(simplex.map(point => this.map(panel, point)), {
        closed: simplex.length > 2,
        fill: simplex.length > 2 ? '#ffe3e3' : 'none',
        'fill-opacity': 0.35,
        stroke: '#e03131',
        'data-gjk-simplex': 'active',
      });
    }

    simplex.forEach((point, i) => {
      this.addCircle(this.map(panel, point), i === 0 ? 6 : 5, {
        fill: i === 0 ? '#e03131' : '#ff8787',
        stroke: '#ffffff',
        'data-gjk-simplex-point': String(i),
      });
    });

    this.addCircle(this.map(panel, supportPoint), 7, {
      fill: '#e03131',
      stroke: '#111',
      'data-gjk-support-point': 'A-minus-B',
    });

    const origin = this.map(panel, Vector.null);
    this.addCircle(origin, 5, {
      fill: '#111',
      stroke: '#111',
      'data-gjk-origin': '1',
    });
    this.addText(origin.x + 8, origin.y - 8, 'origin', {
      'font-size': 12,
      fill: '#555',
    });

    if (simplex.length > 0) {
      const closestScreen = this.map(panel, closest);
      this.addLine(origin, closestScreen, {
        stroke: '#555',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'stroke-dasharray': '5 4',
        'data-gjk-closest-vector': '1',
      });
      this.addCircle(closestScreen, 4.5, {
        fill: '#ffffff',
        stroke: '#555',
        'data-gjk-closest-point': '1',
      });
    }

    this.addText(panel.x + 18, panel.y + panel.height - 44,
      `step ${index}: simplex has ${simplex.length} point${simplex.length === 1 ? '' : 's'}`, {
        'font-size': 12,
        fill: '#555',
      });
    this.addText(panel.x + 18, panel.y + panel.height - 22,
      'move the simplex toward the origin', {
        'font-size': 12,
        fill: '#555',
      });
  }

  renderAxes(panel) {
    const origin = this.map(panel, Vector.null);
    this.addLine({ x: panel.x + 14, y: origin.y }, { x: panel.x + panel.width - 14, y: origin.y }, {
      stroke: '#aaaaaa',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'data-gjk-axis': 'x',
    });
    this.addLine({ x: origin.x, y: panel.y + 38 }, { x: origin.x, y: panel.y + panel.height - 14 }, {
      stroke: '#aaaaaa',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'data-gjk-axis': 'y',
    });
  }

  renderExplanation(shown, index) {
    const support = shown.support;
    this.addText(34, 288, 'support(A - B, v) = supportA(v) - supportB(-v)', {
      'font-size': 13,
      'font-family': 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace',
      'data-gjk-formula': 'support-difference',
    });
    this.addText(428, 288,
      `new point = (${support.point.x.toFixed(2)}, ${support.point.y.toFixed(2)})`, {
        'font-size': 13,
        'font-family': 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace',
        fill: '#e03131',
        'data-gjk-step-readout': String(index),
      });
  }

  supportDirections() {
    return [
      new Vector(1.0, -0.15),
      new Vector(0.2, -1.0),
      new Vector(-0.55, 0.85),
      new Vector(-1.0, -0.1),
      new Vector(0.35, 1.0),
    ];
  }

  supportSteps(shapeA, shapeB) {
    const points = [];
    return this.supportDirections().map((direction) => {
      const support = this.supportDifference(shapeA, shapeB, direction);
      points.unshift(support.point);
      return {
        direction,
        support,
        simplex: points.slice(),
      };
    });
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
    if (simplex.length === 2) {
      return FigureGeometry.closestPointOnSegment(Vector.null, simplex[0], simplex[1]);
    }
    if (FigureGeometry.pointInTriangle(Vector.null, simplex[0], simplex[1], simplex[2])) return Vector.null;

    const candidates = [
      FigureGeometry.closestPointOnSegment(Vector.null, simplex[0], simplex[1]),
      FigureGeometry.closestPointOnSegment(Vector.null, simplex[1], simplex[2]),
      FigureGeometry.closestPointOnSegment(Vector.null, simplex[2], simplex[0]),
    ];
    return candidates.reduce((best, candidate) =>
      candidate.length() < best.length() ? candidate : best
    );
  }

}

((scriptElement) => {
  const figure = new SupportMappingGJK();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
