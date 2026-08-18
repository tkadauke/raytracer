// Interactive widget for render::Instance's transform contract. It
// shows the same ray in world space and in the wrapped primitive's
// local space, then compares the different transform rules for
// points, directions, and normals under non-uniform scale.

class InstanceTransformNormals {
  constructor() {
    this.width = 720;
    this.height = 320;
    this.viewBox = '-6 -3.6 12 7.2';
    this.scaleX = 2.1;
    this.scaleY = 0.75;
    this.rotation = 28;
    this.mode = 'normal';
    this.worldRayOrigin = new Vector(-4.8, 2.0);
    this.worldRayEnd = new Vector(-1.3, -1.1);
    this.localOffset = new Vector(3.0, 0.0);
    this.worldOffset = new Vector(-3.0, 0.0);
    this.arrowId = 'instance-transform-normals-arrow';
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'instance-transform-normals-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: this.viewBox,
    });

    this.widget.addControl(new FigureSliderControl({
      label: 'x scale',
      min: 0.6,
      max: 2.8,
      step: 0.1,
      value: this.scaleX,
      precision: 1,
      onChange: (value) => {
        this.scaleX = value;
        this.render();
      },
    }).element());
    this.widget.addControl(new FigureSliderControl({
      label: 'rotation',
      min: -60,
      max: 60,
      step: 1,
      value: this.rotation,
      precision: 0,
      format: value => `${value.toFixed(0)} deg`,
      onChange: (value) => {
        this.rotation = value;
        this.render();
      },
    }).element());
    this.widget.addControl(new FigureSegmentedControl({
      label: 'view',
      value: this.mode,
      options: [
        { label: 'Point', value: 'point' },
        { label: 'Direction', value: 'direction' },
        { label: 'Normal', value: 'normal' },
      ],
      onChange: (value) => {
        this.mode = value;
        this.render();
      },
    }).element());

    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  clampPoint(point) {
    return new Vector(
      FigureMath.clamp(point.x, -5.6, -0.4),
      FigureMath.clamp(point.y, -3.1, 3.1)
    );
  }

  angle() {
    return this.rotation * degrees;
  }

  transformVector(vector) {
    return new Vector(vector.x * this.scaleX, vector.y * this.scaleY).rotated(this.angle());
  }

  inverseTransformVector(vector) {
    const unrotated = vector.rotated(-this.angle());
    return new Vector(unrotated.x / this.scaleX, unrotated.y / this.scaleY);
  }

  inverseTransposeNormal(vector) {
    const unscaled = new Vector(vector.x / this.scaleX, vector.y / this.scaleY);
    return unscaled.rotated(this.angle()).normalized();
  }

  naiveNormal(vector) {
    return this.transformVector(vector).normalized();
  }

  perpendicular(vector) {
    return new Vector(-vector.y, vector.x).normalized();
  }

  worldPoint(localPoint) {
    return this.worldOffset.plus(this.transformVector(localPoint));
  }

  localPoint(worldPoint) {
    return this.inverseTransformVector(worldPoint.minus(this.worldOffset));
  }

  worldRayDirection() {
    return this.worldRayEnd.minus(this.worldRayOrigin).normalized();
  }

  localRayOrigin() {
    return this.localPoint(this.worldRayOrigin).plus(this.localOffset);
  }

  localRayDirection() {
    return this.inverseTransformVector(this.worldRayDirection()).normalized();
  }

  surfaceLocalPoint() {
    return new Vector(0.72, -0.42).normalized();
  }

  surfaceLocalTangent() {
    return this.perpendicular(this.surfaceLocalPoint());
  }

  surfaceWorldPoint() {
    return this.worldPoint(this.surfaceLocalPoint());
  }

  surfaceWorldTangent() {
    return this.transformVector(this.surfaceLocalTangent()).normalized();
  }

  geometricNormal() {
    const normal = this.perpendicular(this.surfaceWorldTangent());
    const expected = this.inverseTransposeNormal(this.surfaceLocalPoint());
    return normal.dot(expected) < 0 ? normal.multiply(-1) : normal;
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
      'stroke-width': FigureStrokeWidth,
      ...attrs,
    });
  }

  addArrow(start, direction, attrs = {}) {
    this.addLine(start, start.plus(direction), {
      'marker-end': `url(#${this.arrowId})`,
      ...attrs,
    });
  }

  addText(point, text, attrs = {}) {
    const element = this.canvas.add('text', {
      x: point.x,
      y: point.y,
      'font-size': 0.26,
      'font-family': 'sans-serif',
      fill: '#222',
      'pointer-events': 'none',
      ...attrs,
    });
    element.textContent = text;
    return element;
  }

  ellipsePath() {
    const points = [];
    for (let i = 0; i <= 96; i++) {
      const angle = (i / 96) * Math.PI * 2;
      points.push(this.worldPoint(new Vector(Math.cos(angle), Math.sin(angle))));
    }
    return Path.polyline(points);
  }

  circlePath(offset) {
    const points = [];
    for (let i = 0; i <= 96; i++) {
      const angle = (i / 96) * Math.PI * 2;
      points.push(offset.plus(new Vector(Math.cos(angle), Math.sin(angle))));
    }
    return Path.polyline(points);
  }

  renderPanelLabels() {
    this.addText(new Vector(-5.65, -3.15), 'world space');
    this.addText(new Vector(0.35, -3.15), 'local space');
    this.addText(new Vector(-5.65, 3.15), 'drag ray handles');
  }

  renderObjects() {
    this.canvas.add('path', {
      d: this.ellipsePath(),
      fill: '#e7f5ff',
      stroke: '#1864ab',
      'stroke-width': FigureStrokeWidth,
    });
    this.canvas.add('path', {
      d: this.circlePath(this.localOffset),
      fill: '#fff4e6',
      stroke: '#d9480f',
      'stroke-width': FigureStrokeWidth,
    });
    this.addText(this.worldOffset.plus(new Vector(-1.0, 1.55)), 'transformed object');
    this.addText(this.localOffset.plus(new Vector(-0.65, 1.55)), 'wrapped primitive');
  }

  renderRays() {
    const worldDir = this.worldRayDirection();
    const localOrigin = this.localRayOrigin();
    const localDir = this.localRayDirection();

    this.addLine(this.worldRayOrigin.plus(worldDir.multiply(-8)), this.worldRayOrigin.plus(worldDir.multiply(8)), {
      stroke: '#222',
    });
    this.addArrow(this.worldRayOrigin, worldDir.multiply(1.1), {
      stroke: '#222',
    });
    this.addText(this.worldRayOrigin.plus(new Vector(0.12, -0.25)), 'world ray');

    this.addLine(localOrigin.plus(localDir.multiply(-8)), localOrigin.plus(localDir.multiply(8)), {
      stroke: '#d9480f',
    });
    this.addArrow(localOrigin, localDir.multiply(1.1), {
      stroke: '#d9480f',
    });
    this.addText(localOrigin.plus(new Vector(0.12, -0.25)), 'local-space ray', {
      fill: '#a33a00',
    });

    [
      ['ray-origin', this.worldRayOrigin, '#ffe066'],
      ['ray-end', this.worldRayEnd, '#ffd43b'],
    ].forEach(([name, point, fill]) => {
      const handle = new FigureDraggablePoint({
        canvas: this.canvas,
        point,
        radius: 0.16,
        attrs: {
          fill,
          stroke: '#111',
          'stroke-width': FigureStrokeWidth,
          'data-drag-handle': name,
        },
        onDrag: (dragged) => {
          const clamped = this.clampPoint(new Vector(dragged.x, dragged.y));
          if (name === 'ray-origin') {
            this.worldRayOrigin = clamped;
          } else if (clamped.minus(this.worldRayOrigin).length() > 0.4) {
            this.worldRayEnd = clamped;
          }
          this.render();
        },
      });
      this.canvas.append(handle.element());
    });
  }

  renderPointView() {
    const local = this.surfaceLocalPoint().plus(this.localOffset);
    const world = this.surfaceWorldPoint();
    this.addLine(local, world, {
      stroke: '#666',
      'stroke-width': FigureGuideStrokeWidth,
      'stroke-dasharray': '0.12 0.12',
    });
    this.canvas.add('circle', {
      cx: local.x,
      cy: local.y,
      r: 0.11,
      fill: '#d9480f',
      stroke: '#111',
      'stroke-width': FigureStrokeWidth,
      'data-transform-view': 'point',
    });
    this.canvas.add('circle', {
      cx: world.x,
      cy: world.y,
      r: 0.11,
      fill: '#1864ab',
      stroke: '#111',
      'stroke-width': FigureStrokeWidth,
      'data-transform-view': 'point',
    });
    this.addText(new Vector(-5.65, -2.65), 'point: full matrix maps local surface points into world space');
  }

  renderDirectionView() {
    const worldStart = this.worldRayOrigin;
    const localStart = this.localRayOrigin();
    this.addArrow(worldStart, this.worldRayDirection().multiply(0.9), {
      stroke: '#1864ab',
      'data-transform-view': 'direction',
    });
    this.addArrow(localStart, this.localRayDirection().multiply(0.9), {
      stroke: '#d9480f',
      'data-transform-view': 'direction',
    });
    this.addText(new Vector(-5.65, -2.65), 'direction: inverse linear transform maps the world ray direction into local space');
  }

  renderNormalView() {
    const localPoint = this.surfaceLocalPoint();
    const localNormal = localPoint.normalized();
    const worldPoint = this.surfaceWorldPoint();
    const localStart = this.localOffset.plus(localPoint);
    const inverseTranspose = this.inverseTransposeNormal(localNormal);
    const geometric = this.geometricNormal();
    const naive = this.naiveNormal(localNormal);

    this.addArrow(localStart, localNormal.multiply(0.85), {
      stroke: '#20a050',
      'data-transform-view': 'normal',
    });
    this.addText(localStart.plus(localNormal.multiply(1.08)), 'local normal', {
      fill: '#147a3a',
    });

    this.addArrow(worldPoint, geometric.multiply(0.9), {
      stroke: '#20a050',
      'data-transform-view': 'geometric-normal',
    });
    this.addText(worldPoint.plus(geometric.multiply(1.15)), 'geometric normal', {
      fill: '#147a3a',
    });

    this.addArrow(worldPoint, inverseTranspose.multiply(1.25), {
      stroke: '#862e9c',
      'stroke-dasharray': '0.12 0.12',
      'data-transform-view': 'inverse-transpose-normal',
    });
    this.addText(worldPoint.plus(inverseTranspose.multiply(1.55)), 'inverse-transpose normal', {
      fill: '#862e9c',
    });

    this.addArrow(worldPoint, naive.multiply(0.75), {
      stroke: '#c92a2a',
      'stroke-dasharray': '0.1 0.1',
      'data-transform-view': 'naive-normal',
    });
    this.addText(worldPoint.plus(naive.multiply(0.98)), 'scaled like direction', {
      fill: '#a61e1e',
    });
  }

  render() {
    this.canvas.clear();
    this.addArrowMarker();
    this.renderPanelLabels();
    this.renderObjects();
    this.renderRays();

    if (this.mode === 'point') {
      this.renderPointView();
    } else if (this.mode === 'direction') {
      this.renderDirectionView();
    } else {
      this.renderNormalView();
    }
  }
}

((scriptElement) => {
  const figure = new InstanceTransformNormals();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
