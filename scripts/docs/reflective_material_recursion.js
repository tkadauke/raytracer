class ReflectiveMaterialRecursion {
  constructor() {
    this.width = 520;
    this.height = 300;
    this.viewBox = '-4.6 -3.1 10.2 6.2';
    this.hitPoint = new Vector(0, 0);
    this.rayStart = new Vector(-3.2, -1.8);
    this.normalEnd = new Vector(-0.45, -1.15);
    this.reflectionCoefficient = 0.75;
    this.arrowId = 'reflective-material-recursion-arrow';
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'reflective-material-recursion-widget' });
    this.coefficientControl = new FigureSliderControl({
      label: 'reflectionCoefficient',
      min: 0,
      max: 1,
      step: 0.05,
      value: this.reflectionCoefficient,
      precision: 2,
      onChange: (value) => {
        this.reflectionCoefficient = value;
        this.render();
      },
    });
    this.widget.addControl(this.coefficientControl.element());

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
      this.clamp(point.x, -4.2, 1.2),
      this.clamp(point.y, -2.6, 2.6)
    );
  }

  incomingDirection() {
    return this.hitPoint.minus(this.rayStart).normalized();
  }

  normal() {
    return this.normalEnd.minus(this.hitPoint).normalized();
  }

  reflectedDirection() {
    const d = this.incomingDirection();
    const n = this.normal();
    return d.minus(n.multiply(2 * d.dot(n))).normalized();
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
    return this.canvas.add('line', {
      x1: start.x,
      y1: start.y,
      x2: end.x,
      y2: end.y,
      stroke: '#111',
      'stroke-width': FigureStrokeWidth,
      ...attrs,
    });
  }

  addText(position, text, attrs = {}) {
    return this.canvas.add('text', {
      x: position.x,
      y: position.y,
      fill: '#222',
      'font-size': 0.28,
      textContent: text,
      ...attrs,
    });
  }

  addSurface() {
    const n = this.normal();
    const tangent = new Vector(-n.y, n.x);
    this.addLine(tangent.multiply(-2.2), tangent.multiply(2.2), {
      stroke: '#555',
    });
    this.addLine(this.hitPoint, this.hitPoint.plus(n.multiply(1.35)), {
      stroke: '#2060d0',
      'marker-end': `url(#${this.arrowId})`,
      'data-vector': 'normal',
    });
    this.addText(this.hitPoint.plus(n.multiply(1.55)), 'normal', {
      fill: '#2060d0',
    });
    this.canvas.add('circle', {
      cx: this.hitPoint.x,
      cy: this.hitPoint.y,
      r: 0.08,
      fill: '#111',
      stroke: '#111',
      'stroke-width': FigureStrokeWidth,
    });
  }

  addMirrorRays() {
    const d = this.incomingDirection();
    const r = this.reflectedDirection();
    this.addLine(this.rayStart, this.hitPoint, {
      stroke: '#111',
      'marker-end': `url(#${this.arrowId})`,
      'data-ray': 'incoming',
    });
    this.addLine(this.hitPoint, this.hitPoint.plus(r.multiply(3.0)), {
      stroke: '#20a050',
      'marker-end': `url(#${this.arrowId})`,
      'data-ray': 'mirror',
    });
    this.addLine(this.hitPoint.plus(d.multiply(-1.5)), this.hitPoint.plus(r.multiply(1.5)), {
      stroke: '#777',
      'stroke-width': FigureGuideStrokeWidth,
      'stroke-dasharray': '0.12 0.12',
    });
    this.addText(this.rayStart.plus(new Vector(-0.55, -0.25)), 'incoming ray');
    this.addText(this.hitPoint.plus(r.multiply(3.18)).plus(new Vector(-0.2, -0.18)),
                 'mirror ray', { fill: '#20a050' });
    this.addText(new Vector(-3.85, 2.35), 'r = d - 2(d dot n)n', {
      'data-readout': 'mirror-formula',
    });
  }

  addDragHandles() {
    const normalHandle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.normalEnd,
      radius: 0.15,
      attrs: {
        fill: '#dbeafe',
        stroke: '#111',
        'stroke-width': FigureStrokeWidth,
        'data-drag-handle': 'surface-normal',
      },
      onDrag: (dragged) => {
        const point = this.clampPoint(new Vector(dragged.x, dragged.y));
        if (point.minus(this.hitPoint).length() > 0.35) {
          this.normalEnd = point;
          this.render();
        }
      },
    });
    this.canvas.append(normalHandle.element());

    const incomingHandle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.rayStart,
      radius: 0.15,
      attrs: {
        fill: '#fff3bf',
        stroke: '#111',
        'stroke-width': FigureStrokeWidth,
        'data-drag-handle': 'incoming-ray',
      },
      onDrag: (dragged) => {
        const point = this.clampPoint(new Vector(dragged.x, dragged.y));
        if (point.minus(this.hitPoint).length() > 0.6) {
          this.rayStart = point;
          this.render();
        }
      },
    });
    this.canvas.append(incomingHandle.element());
  }

  branchOpacity(depth) {
    return Math.max(0.14, Math.pow(this.reflectionCoefficient, depth));
  }

  addRecursionTree() {
    const x = 3.15;
    const ys = [-1.9, -0.75, 0.4, 1.55];
    this.addText(new Vector(2.15, -2.45), 'recursive rayColor calls');

    for (let i = 0; i < ys.length; i++) {
      const opacity = this.branchOpacity(i);
      this.canvas.add('circle', {
        cx: x,
        cy: ys[i],
        r: 0.2,
        fill: '#20a050',
        stroke: '#111',
        'stroke-width': FigureStrokeWidth,
        opacity,
        'data-recursion-depth': i,
      });
      this.addText(new Vector(x + 0.32, ys[i] + 0.09), `depth ${i}`, {
        opacity,
      });
      if (i < ys.length - 1) {
        this.addLine(new Vector(x, ys[i] + 0.2), new Vector(x, ys[i + 1] - 0.2), {
          stroke: '#20a050',
          'stroke-width': FigureGuideStrokeWidth,
          opacity: this.branchOpacity(i + 1),
          'marker-end': `url(#${this.arrowId})`,
          'data-recursion-edge': i,
        });
      }
    }

    this.addText(new Vector(3.62, 1.95), `weight x ${this.reflectionCoefficient.toFixed(2)} per bounce`, {
      'data-readout': 'recursion-weight',
    });
  }

  render() {
    this.canvas.clear();
    this.addArrowMarker();
    this.addSurface();
    this.addMirrorRays();
    this.addRecursionTree();
    this.addDragHandles();
  }
}

((scriptElement) => {
  const figure = new ReflectiveMaterialRecursion();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
