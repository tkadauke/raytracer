class RayProject {
  constructor() {
    this.width = 320;
    this.height = 240;
    this.bounds = {
      minX: -5.5,
      minY: -4,
      maxX: 5.1667,
      maxY: 4,
    };
    this.viewBox = `${this.bounds.minX} ${this.bounds.minY} 10.6667 8`;
    this.angle = 34 * degrees;
    this.numPoints = 8;
    this.origin = Vector.null;
    this.arrowId = 'ray-project-arrow';
    this.points = Array.from({ length: this.numPoints }, () => (
      new Vector(Math.random() * 10 - 5.5, -Math.random() * 8 + 4)
    ));
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'ray-project-widget' });
    this.widget.setControlsVisible(false);
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: this.viewBox,
    });
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  clampPoint(point) {
    return new Vector(
      this.clamp(point.x, this.bounds.minX, this.bounds.maxX),
      this.clamp(point.y, this.bounds.minY, this.bounds.maxY)
    );
  }

  direction() {
    return Vector.up.rotated(this.angle);
  }

  projectionRay() {
    return new Ray(this.origin, this.direction(), true);
  }

  addArrowMarker() {
    const defs = createSvgElement('defs');
    const marker = createSvgElement('marker', {
      id: this.arrowId,
      markerWidth: 10,
      markerHeight: 10,
      refX: 8,
      refY: 3,
      orient: 'auto',
      markerUnits: 'strokeWidth',
    });
    marker.appendChild(createSvgElement('path', {
      d: 'M0,0 L0,6 L9,3 z',
      fill: '#111',
    }));
    defs.appendChild(marker);
    this.canvas.append(defs);
  }

  addLine(start, end, attrs = {}) {
    this.canvas.add('line', {
      x1: start.x,
      y1: start.y,
      x2: end.x,
      y2: end.y,
      stroke: '#111',
      'stroke-width': 0.05,
      ...attrs,
    });
  }

  addCircle(center, radius, attrs = {}) {
    this.canvas.add('circle', {
      cx: center.x,
      cy: center.y,
      r: radius,
      fill: 'none',
      stroke: '#111',
      'stroke-width': 0.05,
      ...attrs,
    });
  }

  renderAxes() {
    this.addLine(new Vector(this.bounds.minX + 0.3, 0), new Vector(this.bounds.maxX - 0.3, 0), {
      'marker-end': `url(#${this.arrowId})`,
    });
    this.addLine(new Vector(0, this.bounds.maxY - 0.3), new Vector(0, this.bounds.minY + 0.3), {
      'marker-end': `url(#${this.arrowId})`,
    });
    this.canvas.add('text', {
      x: this.bounds.maxX - 0.55,
      y: 0.38,
      'font-size': 0.34,
      fill: '#222',
      textContent: 'x',
    });
    this.canvas.add('text', {
      x: 0.28,
      y: this.bounds.minY + 0.55,
      'font-size': 0.34,
      fill: '#222',
      textContent: 'y',
    });
  }

  renderProjectionLine() {
    const direction = this.direction();
    this.addLine(this.origin.plus(direction.multiply(-50)), this.origin.plus(direction.multiply(50)));
    this.addLine(this.origin, this.origin.plus(direction.multiply(1.6)), {
      'marker-end': `url(#${this.arrowId})`,
    });
  }

  renderPoint(point, index, projection) {
    const projected = projection.projected(point);
    this.addLine(point, projected, {
      stroke: '#777',
      'stroke-width': 0.035,
      'stroke-dasharray': '0.12 0.12',
    });
    this.addCircle(projected, 0.08, {
      fill: '#111',
      stroke: '#111',
      'pointer-events': 'none',
    });

    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point,
      radius: 0.15,
      attrs: {
        fill: '#f03e3e',
        stroke: '#111',
        'stroke-width': 0.05,
        'data-drag-handle': 'project-point',
        'data-point-index': index,
      },
      onDrag: (dragged) => {
        this.points[index] = this.clampPoint(new Vector(dragged.x, dragged.y));
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }

  render() {
    this.canvas.clear();
    this.addArrowMarker();
    this.renderAxes();
    this.renderProjectionLine();

    const projection = this.projectionRay();
    this.points.forEach((point, index) => this.renderPoint(point, index, projection));
  }
}

((scriptElement) => {
  const figure = new RayProject();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
