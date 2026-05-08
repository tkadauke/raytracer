// Interactive widget for transparent-material refraction and total internal
// reflection.
//
// The diagram shows an internal ray striking a flat surface. The upward normal
// points from the inner medium to the outer medium, so Snell's law is:
//
//   innerIor * sin(incidentAngle) = outerIor * sin(transmittedAngle)
//
// When the inner medium has the higher IOR, the critical angle marks the
// largest incident angle that can still transmit. Drag the incident ray or
// change either IOR to see the refracted branch disappear under total internal
// reflection.

class TransparentMaterialRefraction {
  constructor() {
    this.width = 640;
    this.height = 360;
    this.hit = { x: 320, y: 178 };
    this.incidentSource = { x: 230, y: 330 };
    this.innerIor = 1.5;
    this.outerIor = 1.0;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'transparent-material-refraction-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.innerControl = new FigureSliderControl({
      label: 'inner IOR',
      min: 1.0,
      max: 2.5,
      step: 0.01,
      value: this.innerIor,
      precision: 2,
      onChange: (value) => {
        this.innerIor = value;
        this.render();
      },
    });
    this.outerControl = new FigureSliderControl({
      label: 'outer IOR',
      min: 1.0,
      max: 1.6,
      step: 0.01,
      value: this.outerIor,
      precision: 2,
      onChange: (value) => {
        this.outerIor = value;
        this.render();
      },
    });

    this.widget.addControl(this.innerControl.element());
    this.widget.addControl(this.outerControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  vector(from, to) {
    return {
      x: to.x - from.x,
      y: to.y - from.y,
    };
  }

  length(vector) {
    return Math.sqrt(vector.x * vector.x + vector.y * vector.y);
  }

  normalize(vector) {
    const length = this.length(vector);
    return {
      x: vector.x / length,
      y: vector.y / length,
    };
  }

  dot(a, b) {
    return a.x * b.x + a.y * b.y;
  }

  incidentDirection() {
    return this.normalize(this.vector(this.incidentSource, this.hit));
  }

  incidentAngle() {
    const incoming = this.incidentDirection();
    const normal = { x: 0, y: -1 };
    return Math.acos(this.clamp(this.dot(incoming, normal), -1, 1));
  }

  criticalAngle() {
    if (this.innerIor <= this.outerIor) return null;
    return Math.asin(this.outerIor / this.innerIor);
  }

  state() {
    const thetaI = this.incidentAngle();
    const sinThetaT = (this.innerIor / this.outerIor) * Math.sin(thetaI);
    const tir = sinThetaT > 1.0;
    return {
      thetaI,
      sinThetaT,
      tir,
      thetaT: tir ? null : Math.asin(this.clamp(sinThetaT, -1, 1)),
      critical: this.criticalAngle(),
    };
  }

  pointOnLowerAngle(angle, radius, sign) {
    return {
      x: this.hit.x + sign * Math.sin(angle) * radius,
      y: this.hit.y + Math.cos(angle) * radius,
    };
  }

  pointOnUpperAngle(angle, radius, sign) {
    return {
      x: this.hit.x + sign * Math.sin(angle) * radius,
      y: this.hit.y - Math.cos(angle) * radius,
    };
  }

  arcPath(radius, angle, side, sign) {
    const start = side === 'upper'
      ? this.pointOnUpperAngle(0, radius, sign)
      : this.pointOnLowerAngle(0, radius, sign);
    const end = side === 'upper'
      ? this.pointOnUpperAngle(angle, radius, sign)
      : this.pointOnLowerAngle(angle, radius, sign);
    const sweep = side === 'upper'
      ? (sign < 0 ? 0 : 1)
      : (sign < 0 ? 1 : 0);
    return `M ${start.x} ${start.y} A ${radius} ${radius} 0 0 ${sweep} ${end.x} ${end.y}`;
  }

  addArrowDefs() {
    const defs = this.canvas.add('defs');
    defs.innerHTML = `
      <marker id="transparent-refraction-arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#222"></path>
      </marker>
      <marker id="transparent-refraction-red-arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#d9480f"></path>
      </marker>
      <marker id="transparent-refraction-blue-arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#1c7ed6"></path>
      </marker>
      <marker id="transparent-refraction-green-arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L0,6 L9,3 z" fill="#2b8a3e"></path>
      </marker>
    `;
  }

  addText(x, y, text, attrs = {}) {
    const element = this.canvas.add('text', {
      x,
      y,
      'font-family': 'sans-serif',
      'font-size': 14,
      fill: '#222',
      ...attrs,
    });
    element.textContent = text;
    return element;
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

  addRay(from, direction, length, attrs = {}) {
    const to = {
      x: from.x + direction.x * length,
      y: from.y + direction.y * length,
    };
    return this.addLine(from, to, {
      'marker-end': 'url(#transparent-refraction-arrow)',
      ...attrs,
    });
  }

  render() {
    this.canvas.clear();
    this.addArrowDefs();
    this.renderMedia();
    this.renderRays();
    this.renderReadout();
  }

  renderMedia() {
    this.canvas.add('rect', {
      x: 0,
      y: 0,
      width: this.width,
      height: this.hit.y,
      fill: '#eef7ff',
    });
    this.canvas.add('rect', {
      x: 0,
      y: this.hit.y,
      width: this.width,
      height: this.height - this.hit.y,
      fill: '#fff7e6',
    });
    this.addLine({ x: 42, y: this.hit.y }, { x: 598, y: this.hit.y }, {
      stroke: '#333',
      'stroke-width': FigurePixelStrokeWidth,
    });
    this.addText(46, 30, `outer medium (IOR ${this.outerIor.toFixed(2)})`);
    this.addText(46, 342, `inner medium (IOR ${this.innerIor.toFixed(2)})`);

    this.addRay(this.hit, { x: 0, y: -1 }, 94, {
      stroke: '#111',
      'marker-end': 'url(#transparent-refraction-arrow)',
    });
    this.addLine(this.hit, { x: this.hit.x, y: this.hit.y + 112 }, {
      stroke: '#666',
      'stroke-dasharray': '5 5',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.addText(this.hit.x + 10, this.hit.y - 86, 'surface normal');
  }

  renderRays() {
    const state = this.state();
    const incoming = this.incidentDirection();
    const sign = incoming.x < 0 ? -1 : 1;
    const normal = { x: 0, y: -1 };
    const reflect = {
      x: incoming.x,
      y: incoming.y - 2 * this.dot(incoming, normal) * normal.y,
    };

    this.addLine(this.incidentSource, this.hit, {
      stroke: '#d9480f',
      'marker-end': 'url(#transparent-refraction-red-arrow)',
      'data-ray': 'incident',
    });
    this.addRay(this.hit, reflect, 145, {
      stroke: '#1c7ed6',
      'marker-end': 'url(#transparent-refraction-blue-arrow)',
      'data-ray': 'reflected',
    });

    if (!state.tir) {
      const trans = {
        x: sign * Math.sin(state.thetaT),
        y: -Math.cos(state.thetaT),
      };
      this.addRay(this.hit, trans, 170, {
        stroke: '#2b8a3e',
        'marker-end': 'url(#transparent-refraction-green-arrow)',
        'data-ray': 'refracted',
      });
      this.addText(this.hit.x + 120 * trans.x + 10, this.hit.y + 120 * trans.y, 'refracted');
    }

    this.canvas.add('path', {
      d: this.arcPath(44, state.thetaI, 'lower', sign),
      fill: 'transparent',
      stroke: '#d9480f',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'data-angle': 'incident',
    });
    this.addText(this.hit.x + sign * 34, this.hit.y + 62, 'theta i');

    if (!state.tir) {
      this.canvas.add('path', {
        d: this.arcPath(50, state.thetaT, 'upper', sign),
        fill: 'transparent',
        stroke: '#2b8a3e',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-angle': 'refracted',
      });
      this.addText(this.hit.x + sign * 42, this.hit.y - 58, 'theta t');
    }

    if (state.critical !== null) {
      const left = this.pointOnLowerAngle(state.critical, 126, -1);
      const right = this.pointOnLowerAngle(state.critical, 126, 1);
      this.addLine(this.hit, left, {
        stroke: '#7048e8',
        'stroke-dasharray': '6 5',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-critical-angle-marker': 'left',
      });
      this.addLine(this.hit, right, {
        stroke: '#7048e8',
        'stroke-dasharray': '6 5',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-critical-angle-marker': 'right',
      });
      this.addText(this.hit.x + 82, this.hit.y + 104, `critical ${this.degrees(state.critical).toFixed(1)} deg`, {
        fill: '#4c1d95',
      });
    } else {
      this.addText(this.hit.x + 74, this.hit.y + 104, 'no critical angle', {
        fill: '#555',
      });
    }

    this.addText(this.incidentSource.x - 64, this.incidentSource.y - 12, 'incident');
    this.addText(this.hit.x + reflect.x * 98 + 8, this.hit.y + reflect.y * 98, 'reflected');
    this.canvas.add('circle', {
      cx: this.hit.x,
      cy: this.hit.y,
      r: 5,
      fill: '#111',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
    });

    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.incidentSource,
      radius: 9,
      attrs: {
        fill: '#ffd8a8',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'incident-direction',
      },
      onDrag: (point) => {
        this.incidentSource = {
          x: this.clamp(point.x, 70, 570),
          y: this.clamp(point.y, this.hit.y + 45, 340),
        };
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }

  renderReadout() {
    const state = this.state();
    const x = 418;
    const y = 64;
    const incident = this.degrees(state.thetaI);

    this.canvas.add('rect', {
      x: x - 16,
      y: y - 28,
      width: 188,
      height: 116,
      rx: 6,
      fill: '#ffffff',
      stroke: '#c8c8c8',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.addText(x, y, `theta i = ${incident.toFixed(1)} deg`, {
      'font-family': 'monospace',
      'data-readout': 'incident-angle',
    });
    this.addText(x, y + 24, `${this.innerIor.toFixed(2)} sin(theta i) = ${this.outerIor.toFixed(2)} sin(theta t)`, {
      'font-family': 'monospace',
      'font-size': 12,
      'data-readout': 'snell-law',
    });

    if (state.tir) {
      this.addText(x, y + 50, 'total internal reflection', {
        fill: '#c92a2a',
        'font-weight': '700',
        'data-state': 'tir',
      });
      this.addText(x, y + 74, 'transmitted ray disappears', {
        fill: '#c92a2a',
      });
    } else {
      this.addText(x, y + 50, `theta t = ${this.degrees(state.thetaT).toFixed(1)} deg`, {
        fill: '#2b8a3e',
        'font-family': 'monospace',
        'data-state': 'refracting',
      });
      this.addText(x, y + 74, 'reflection and transmission split', {
        fill: '#2b8a3e',
      });
    }
  }

  degrees(radians) {
    return radians / degrees;
  }
}

((scriptElement) => {
  const figure = new TransparentMaterialRefraction();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
