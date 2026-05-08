class BoundingBoxInclude {
  constructor() {
    this.width = 320;
    this.height = 240;
    this.viewBox = '-5.5 -4 10.6667 8';
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.point = new Vector(2.5, 1);
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'bounding-box-include-widget' });
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

  includedBox() {
    const newTopleft = new Vector(
      Math.min(this.point.x, this.topleft.x),
      Math.min(this.point.y, this.topleft.y)
    );
    const oldBottomRight = this.topleft.plus(this.size);
    const newBottomRight = new Vector(
      Math.max(this.point.x, oldBottomRight.x),
      Math.max(this.point.y, oldBottomRight.y)
    );
    return {
      topleft: newTopleft,
      size: newBottomRight.minus(newTopleft),
    };
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

  render() {
    this.canvas.clear();
    this.addRectangle(this.topleft, this.size, true);
    const result = this.includedBox();
    this.addRectangle(result.topleft, result.size, false);

    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.point,
      radius: 0.14,
      attrs: {
        fill: '#f03e3e',
        stroke: '#111',
        'stroke-width': 0.05,
        'data-drag-handle': 'included-point',
      },
      onDrag: (point) => {
        this.point = new Vector(
          this.clamp(point.x, -4.8, 4.8),
          this.clamp(point.y, -3.4, 3.4)
        );
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }
}

((scriptElement) => {
  const figure = new BoundingBoxInclude();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
