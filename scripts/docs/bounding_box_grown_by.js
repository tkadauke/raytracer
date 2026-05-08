class BoundingBoxGrownBy {
  constructor() {
    this.width = 320;
    this.height = 240;
    this.viewBox = '-5.5 -4 10.6667 8';
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.vector = new Vector(0.5, 1);
    this.arrowId = 'bounding-box-grown-by-arrow';
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'bounding-box-grown-by-widget' });
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

  bottomRight() {
    return this.topleft.plus(this.size);
  }

  topRight() {
    return this.topleft.plus(new Vector(this.size.x, 0));
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

  addRectangle(topleft, size, dashed = false) {
    this.canvas.add('rect', {
      x: topleft.x,
      y: topleft.y,
      width: size.x,
      height: size.y,
      fill: 'none',
      stroke: '#111',
      'stroke-width': 0.05,
      'stroke-dasharray': dashed ? '0.14 0.14' : undefined,
    });
  }

  addArrow(start, vector, stroke = '#111') {
    this.canvas.add('line', {
      x1: start.x,
      y1: start.y,
      x2: start.x + vector.x,
      y2: start.y + vector.y,
      stroke,
      'stroke-width': 0.05,
      'marker-end': `url(#${this.arrowId})`,
    });
  }

  render() {
    this.canvas.clear();
    this.addArrowMarker();
    this.addArrow(this.topleft, this.vector.multiply(-1));
    this.addArrow(
      this.topleft.plus(new Vector(this.size.x, 0)),
      new Vector(this.vector.x, -this.vector.y),
      '#d22'
    );
    this.addArrow(
      this.topleft.plus(new Vector(0, this.size.y)),
      new Vector(-this.vector.x, this.vector.y)
    );
    this.addArrow(this.bottomRight(), this.vector);
    this.addRectangle(this.topleft, this.size, true);
    this.addRectangle(this.topleft.minus(this.vector), this.size.plus(this.vector.multiply(2)));

    const topRight = this.topRight();
    const handlePoint = topRight.plus(new Vector(this.vector.x, -this.vector.y));
    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: handlePoint,
      radius: 0.14,
      attrs: {
        fill: '#f03e3e',
        stroke: '#111',
        'stroke-width': 0.05,
        'data-drag-handle': 'growth-vector-end',
      },
      onDrag: (point) => {
        this.vector = new Vector(
          this.clamp(point.x - topRight.x, -1.6, 2.6),
          this.clamp(topRight.y - point.y, -1.6, 2.4)
        );
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }
}

((scriptElement) => {
  const figure = new BoundingBoxGrownBy();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
