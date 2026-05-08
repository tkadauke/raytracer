class BoundingBoxAnd {
  constructor() {
    this.width = 320;
    this.height = 240;
    this.viewBox = '0 -8 10.6667 8';
    this.boxes = [
      { topleft: new Vector(2, -4), size: new Vector(5, 2) },
      { topleft: new Vector(4, -6), size: new Vector(4, 3) },
    ];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'bounding-box-and-widget' });
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

  clampBox(box, topleft) {
    return new Vector(
      this.clamp(topleft.x, 0.2, 10.4 - box.size.x),
      this.clamp(topleft.y, -7.8, -0.2 - box.size.y)
    );
  }

  intersection() {
    const [a, b] = this.boxes;
    const left = Math.max(a.topleft.x, b.topleft.x);
    const top = Math.max(a.topleft.y, b.topleft.y);
    const right = Math.min(a.topleft.x + a.size.x, b.topleft.x + b.size.x);
    const bottom = Math.min(a.topleft.y + a.size.y, b.topleft.y + b.size.y);
    if (right <= left || bottom <= top) return null;
    return {
      topleft: new Vector(left, top),
      size: new Vector(right - left, bottom - top),
    };
  }

  addRectangle(box, attrs = {}) {
    return this.canvas.add('rect', {
      x: box.topleft.x,
      y: box.topleft.y,
      width: box.size.x,
      height: box.size.y,
      ...attrs,
    });
  }

  addSourceBox(box, index) {
    const rect = this.addRectangle(box, {
      fill: '#ffffff',
      'fill-opacity': 0.01,
      stroke: '#111',
      'stroke-width': FigureStrokeWidth,
      'stroke-dasharray': '0.14 0.14',
      cursor: 'move',
      'data-drag-handle': 'source-box',
      'data-box-index': index,
    });
    rect.addEventListener('pointerdown', (event) => {
      const start = this.canvas.pointFromEvent(event);
      const initial = box.topleft;
      const move = (moveEvent) => {
        const point = this.canvas.pointFromEvent(moveEvent);
        this.boxes[index].topleft = this.clampBox(box, new Vector(
          initial.x + point.x - start.x,
          initial.y + point.y - start.y
        ));
        this.render();
        moveEvent.preventDefault();
      };
      const up = () => {
        document.removeEventListener('pointermove', move);
        document.removeEventListener('pointerup', up);
      };
      document.addEventListener('pointermove', move);
      document.addEventListener('pointerup', up);
      event.preventDefault();
    });
  }

  render() {
    this.canvas.clear();
    this.boxes.forEach((box, index) => this.addSourceBox(box, index));
    const intersection = this.intersection();
    if (!intersection) return;
    this.addRectangle(intersection, {
      fill: 'none',
      stroke: '#111',
      'stroke-width': FigureStrokeWidth,
      'pointer-events': 'none',
    });
  }
}

((scriptElement) => {
  const figure = new BoundingBoxAnd();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
