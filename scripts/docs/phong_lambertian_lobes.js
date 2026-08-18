// Interactive widget for Lambertian and Phong BRDF documentation.
// The surface normal is fixed. Drag the light and view directions:
// Lambertian diffuse depends only on max(n dot l, 0), while the Phong
// specular term also depends on whether the view vector aligns with the
// reflected-light lobe. Raising the exponent narrows that lobe.

class PhongLambertianLobes {
  constructor() {
    this.width = 520;
    this.height = 300;
    this.origin = new Vector(250, 210);
    this.vectorRadius = 82;
    this.lobeScale = 92;
    this.normal = new Vector(0, -1);
    this.light = new Vector(-0.62, -0.78).normalized();
    this.view = new Vector(0.52, -0.85).normalized();
    this.diffuseCoefficient = 0.75;
    this.specularCoefficient = 0.8;
    this.exponent = 24;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'phong-lambertian-lobes-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.widget.addControl(new FigureSliderControl({
      label: 'diffuse coefficient',
      min: 0,
      max: 1,
      step: 0.05,
      value: this.diffuseCoefficient,
      precision: 2,
      onChange: (value) => {
        this.diffuseCoefficient = value;
        this.render();
      },
    }).element());

    this.widget.addControl(new FigureSliderControl({
      label: 'specular coefficient',
      min: 0,
      max: 1,
      step: 0.05,
      value: this.specularCoefficient,
      precision: 2,
      onChange: (value) => {
        this.specularCoefficient = value;
        this.render();
      },
    }).element());

    this.widget.addControl(new FigureSliderControl({
      label: 'Phong exponent',
      min: 1,
      max: 128,
      step: 1,
      value: this.exponent,
      precision: 0,
      onChange: (value) => {
        this.exponent = value;
        this.render();
      },
    }).element());

    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  directionFromPoint(point) {
    const offset = new Vector(point.x - this.origin.x, point.y - this.origin.y);
    if (offset.length() < 1e-6) return this.normal;
    const direction = offset.normalized();
    return direction.y > -0.08 ? new Vector(direction.x, -0.08).normalized() : direction;
  }

  pointForDirection(direction, radius = this.vectorRadius) {
    return this.origin.plus(direction.multiply(radius));
  }

  normalDotLight() {
    return Math.max(0, this.normal.dot(this.light));
  }

  reflectionDirection() {
    const nDotL = this.normal.dot(this.light);
    return this.normal.multiply(2 * nDotL).minus(this.light).normalized();
  }

  diffuseTerm() {
    return this.diffuseCoefficient * this.normalDotLight();
  }

  specularTerm() {
    if (this.normalDotLight() <= 0) return 0;
    const alignment = Math.max(0, this.reflectionDirection().dot(this.view));
    return this.specularCoefficient * Math.pow(alignment, this.exponent);
  }

  addArrowMarker(id, color) {
    const defs = this.canvas.add('defs');
    const marker = createSvgElement('marker', {
      id,
      markerWidth: 10,
      markerHeight: 10,
      refX: 8,
      refY: 3,
      orient: 'auto',
      markerUnits: 'strokeWidth',
    });
    marker.appendChild(createSvgElement('path', {
      d: 'M0,0 L0,6 L9,3 z',
      fill: color,
    }));
    defs.appendChild(marker);
  }

  addLine(start, end, attrs = {}) {
    this.canvas.add('line', {
      x1: start.x,
      y1: start.y,
      x2: end.x,
      y2: end.y,
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
      ...attrs,
    });
  }

  addLabel(text, x, y, attrs = {}) {
    const label = this.canvas.add('text', {
      x,
      y,
      'font-size': 13,
      'font-family': 'sans-serif',
      fill: '#222',
      ...attrs,
    });
    label.textContent = text;
    return label;
  }

  addVector(direction, color, label, handleName, onDrag) {
    const endpoint = this.pointForDirection(direction);
    this.addLine(this.origin, endpoint, {
      stroke: color,
      'marker-end': `url(#${handleName}-arrow)`,
    });
    this.addLabel(label, endpoint.x + (direction.x >= 0 ? 8 : -28), endpoint.y - 6, {
      fill: color,
      'font-weight': '600',
    });

    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: endpoint,
      radius: 9,
      attrs: {
        fill: '#ffffff',
        stroke: color,
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': handleName,
      },
      onDrag: (point) => {
        onDrag(this.directionFromPoint(point));
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }

  addLobe() {
    const reflected = this.reflectionDirection();
    const points = [this.origin];
    const steps = 96;
    for (let i = 0; i <= steps; i++) {
      const t = i / steps;
      const angle = Math.PI * (1 - t);
      const direction = new Vector(Math.cos(angle), -Math.sin(angle)).normalized();
      const alignment = Math.max(0, reflected.dot(direction));
      const magnitude = this.specularCoefficient * Math.pow(alignment, this.exponent);
      points.push(this.origin.plus(direction.multiply(18 + magnitude * this.lobeScale)));
    }
    points.push(this.origin);

    this.canvas.add('path', {
      d: Path.polyline(points, { closed: true }),
      fill: '#4dabf7',
      'fill-opacity': 0.18,
      stroke: '#1971c2',
      'stroke-width': FigurePixelStrokeWidth,
      'data-specular-lobe': 'phong',
    });
  }

  addDiffuseMeter() {
    const x = 28;
    const y = 56;
    const width = 128;
    const height = 16;
    const nDotL = this.normalDotLight();
    const diffuse = this.diffuseTerm();

    this.addLabel(`n dot l = ${nDotL.toFixed(2)}`, x, y - 14);
    this.canvas.add('rect', {
      x,
      y,
      width,
      height,
      fill: '#ffffff',
      stroke: '#555',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.canvas.add('rect', {
      x,
      y,
      width: FigureMath.clamp(diffuse, 0, 1) * width,
      height,
      fill: '#ffd43b',
      stroke: 'none',
      'data-diffuse-term': diffuse.toFixed(3),
    });
    this.addLabel(`diffuse = ${diffuse.toFixed(2)}`, x, y + 36);
  }

  addSpecularReadout() {
    this.addLabel(`specular = ${this.specularTerm().toFixed(3)}`, 346, 42);
    this.addLabel('higher exponent: narrower highlight', 300, 64, {
      fill: '#495057',
    });
  }

  render() {
    this.canvas.clear();
    this.addArrowMarker('normal-arrow', '#222');
    this.addArrowMarker('light-vector-arrow', '#f08c00');
    this.addArrowMarker('view-vector-arrow', '#2f9e44');

    this.addLine(new Vector(70, this.origin.y), new Vector(430, this.origin.y), {
      stroke: '#555',
    });
    this.addLabel('surface', 78, this.origin.y + 22, { fill: '#555' });

    const normalEnd = this.pointForDirection(this.normal, 96);
    this.addLine(this.origin, normalEnd, {
      stroke: '#222',
      'marker-end': 'url(#normal-arrow)',
    });
    this.addLabel('n', normalEnd.x + 8, normalEnd.y + 4, { 'font-weight': '600' });

    this.addLobe();
    this.addVector(this.light, '#f08c00', 'l', 'light-vector', (direction) => {
      this.light = direction;
    });
    this.addVector(this.view, '#2f9e44', 'v', 'view-vector', (direction) => {
      this.view = direction;
    });

    const reflectedEnd = this.pointForDirection(this.reflectionDirection(), 66);
    this.addLine(this.origin, reflectedEnd, {
      stroke: '#1971c2',
      'stroke-dasharray': '6 5',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.addLabel('reflection', reflectedEnd.x + 8, reflectedEnd.y + 2, {
      fill: '#1971c2',
    });

    this.addDiffuseMeter();
    this.addSpecularReadout();
  }
}

((scriptElement) => {
  const figure = new PhongLambertianLobes();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
