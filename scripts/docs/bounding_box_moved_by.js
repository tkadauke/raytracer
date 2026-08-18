class BoundingBoxMovedBy {
  constructor() {
    this.width = 320;
    this.height = 240;
    this.viewBox = '-5.5 -4 10.6667 8';
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.vector = new Vector(0.5, 1);
    this.arrowId = 'bounding-box-moved-by-arrow';
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'bounding-box-moved-by-widget' });
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

  corners() {
    return [
      this.topleft,
      this.topleft.plus(new Vector(this.size.x, 0)),
      this.topleft.plus(new Vector(0, this.size.y)),
      this.topleft.plus(this.size),
    ];
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
      'stroke-width': FigureStrokeWidth,
      'stroke-dasharray': dashed ? '0.14 0.14' : undefined,
    });
  }

  addArrow(start, vector) {
    this.canvas.add('line', {
      x1: start.x,
      y1: start.y,
      x2: start.x + vector.x,
      y2: start.y + vector.y,
      stroke: '#111',
      'stroke-width': FigureStrokeWidth,
      'marker-end': `url(#${this.arrowId})`,
    });
  }

  render() {
    this.canvas.clear();
    this.addArrowMarker();
    this.corners().forEach(corner => this.addArrow(corner, this.vector));
    this.addRectangle(this.topleft, this.size, true);
    this.addRectangle(this.topleft.plus(this.vector), this.size, false);

    const movedCorner = this.topleft.plus(this.vector);
    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: movedCorner,
      radius: 0.14,
      attrs: {
        fill: '#f03e3e',
        stroke: '#111',
        'stroke-width': FigureStrokeWidth,
        'data-drag-handle': 'move-vector-end',
      },
      onDrag: (point) => {
        this.vector = new Vector(
          FigureMath.clamp(point.x - this.topleft.x, -2.8, 3.2),
          FigureMath.clamp(point.y - this.topleft.y, -2.2, 3.2)
        );
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }
}

((scriptElement) => {
  const figure = new BoundingBoxMovedBy();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
