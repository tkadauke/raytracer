class CsgHitIntervals {
  constructor() {
    this.width = 520;
    this.height = 260;
    this.bounds = {
      minT: -1.5,
      maxT: 7.0,
      left: 46,
      right: 486,
    };
    this.rows = {
      a: 66,
      b: 112,
      result: 176,
    };
    this.operation = 'union';
    this.intervals = {
      a: { enter: -0.6, exit: 4.5 },
      b: { enter: 1.7, exit: 6.1 },
    };
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'csg-hit-intervals-widget' });
    this.operationControl = new FigureSegmentedControl({
      label: 'operation',
      options: [
        { label: 'union', value: 'union' },
        { label: 'intersection', value: 'intersection' },
        { label: 'difference', value: 'difference' },
      ],
      value: this.operation,
      onChange: (value) => {
        this.operation = value;
        this.render();
      },
    });
    this.widget.addControl(this.operationControl.element());
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  scale() {
    return (this.bounds.right - this.bounds.left) / (this.bounds.maxT - this.bounds.minT);
  }

  xForT(t) {
    return this.bounds.left + (t - this.bounds.minT) * this.scale();
  }

  tForX(x) {
    return this.bounds.minT + (x - this.bounds.left) / this.scale();
  }

  events() {
    return [
      { t: this.intervals.a.enter, shape: 'A', kind: 'enter' },
      { t: this.intervals.a.exit, shape: 'A', kind: 'exit' },
      { t: this.intervals.b.enter, shape: 'B', kind: 'enter' },
      { t: this.intervals.b.exit, shape: 'B', kind: 'exit' },
    ].sort((left, right) => left.t - right.t);
  }

  resultIntervals() {
    const events = this.events();
    const spans = [];
    let inA = false;
    let inB = false;
    let previousT = null;
    let previousInside = false;
    for (const event of events) {
      if (previousT !== null && previousInside && event.t > previousT) {
        spans.push({ enter: previousT, exit: event.t });
      }

      if (event.shape === 'A') {
        inA = event.kind === 'enter';
      } else {
        inB = event.kind === 'enter';
      }
      previousT = event.t;
      previousInside = this.insideResult(inA, inB);
    }
    return spans;
  }

  insideResult(inA, inB) {
    if (this.operation === 'union') return inA || inB;
    if (this.operation === 'intersection') return inA && inB;
    return inA && !inB;
  }

  resultEvents() {
    const events = this.events();
    const result = [];
    let inA = false;
    let inB = false;
    let previousInside = false;
    for (const event of events) {
      const before = previousInside;
      if (event.shape === 'A') {
        inA = event.kind === 'enter';
      } else {
        inB = event.kind === 'enter';
      }
      const after = this.insideResult(inA, inB);
      if (before !== after) {
        result.push({
          t: event.t,
          kind: after ? 'enter' : 'exit',
          source: event.shape,
          flipped: this.operation === 'difference' && event.shape === 'B',
        });
      }
      previousInside = after;
    }
    return result;
  }

  selectedHit() {
    return this.resultEvents().find(event => event.t > 0) || null;
  }

  addLine(x1, y1, x2, y2, attrs = {}) {
    return this.canvas.add('line', {
      x1,
      y1,
      x2,
      y2,
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
      ...attrs,
    });
  }

  addText(x, y, text, attrs = {}) {
    return this.canvas.add('text', {
      x,
      y,
      fill: '#202020',
      'font-size': 14,
      textContent: text,
      ...attrs,
    });
  }

  addIntervalBar(enter, exit, y, attrs = {}) {
    this.canvas.add('rect', {
      x: this.xForT(enter),
      y: y - 10,
      width: this.xForT(exit) - this.xForT(enter),
      height: 20,
      rx: 4,
      fill: '#d9e8ff',
      stroke: '#2060d0',
      'stroke-width': FigurePixelStrokeWidth,
      ...attrs,
    });
  }

  addMarker(t, y, kind, label, attrs = {}) {
    const x = this.xForT(t);
    const points = kind === 'enter'
      ? `${x - 7},${y - 13} ${x + 7},${y} ${x - 7},${y + 13}`
      : `${x + 7},${y - 13} ${x - 7},${y} ${x + 7},${y + 13}`;
    this.canvas.add('polygon', {
      points,
      fill: '#ffffff',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      ...attrs,
    });
    this.addText(x - 15, y - 18, label, {
      'font-size': 12,
      'text-anchor': 'middle',
      'pointer-events': 'none',
    });
  }

  addEndpointHandle(shapeKey, endpointKey, y) {
    const interval = this.intervals[shapeKey];
    const point = { x: this.xForT(interval[endpointKey]), y };
    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point,
      radius: 8,
      attrs: {
        fill: endpointKey === 'enter' ? '#d0ebff' : '#ffe3e3',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'csg-interval-endpoint',
        'data-shape': shapeKey.toUpperCase(),
        'data-endpoint': endpointKey,
      },
      onDrag: (dragged) => {
        const minGap = 0.25;
        const draggedT = this.tForX(dragged.x);
        if (endpointKey === 'enter') {
          interval.enter = this.clamp(draggedT, this.bounds.minT, interval.exit - minGap);
        } else {
          interval.exit = this.clamp(draggedT, interval.enter + minGap, this.bounds.maxT);
        }
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }

  renderAxis() {
    const axisY = 28;
    const defs = this.canvas.add('defs');
    const marker = createSvgElement('marker', {
      id: 'csg-hit-intervals-arrow',
      markerWidth: 10,
      markerHeight: 10,
      refX: 8,
      refY: 3,
      orient: 'auto',
      markerUnits: 'strokeWidth',
    });
    marker.appendChild(createSvgElement('path', {
      d: 'M0,0 L0,6 L9,3 z',
      fill: '#222',
    }));
    defs.appendChild(marker);
    this.addLine(this.bounds.left, axisY, this.bounds.right, axisY, {
      'marker-end': 'url(#csg-hit-intervals-arrow)',
    });
    [0, 2, 4, 6].forEach((t) => {
      const x = this.xForT(t);
      this.addLine(x, axisY - 5, x, axisY + 5);
      this.addText(x, axisY + 22, String(t), {
        'font-size': 12,
        'text-anchor': 'middle',
      });
    });
    const originX = this.xForT(0);
    this.addLine(originX, 44, originX, 212, {
      stroke: '#777',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '4 4',
    });
    this.addText(originX + 6, 58, 't = 0');
    this.addText(this.bounds.right + 8, axisY + 4, 'ray t');
  }

  renderSourceRow(key, y, fill) {
    const interval = this.intervals[key];
    const label = key.toUpperCase();
    this.addText(16, y + 5, label, { 'font-weight': '600' });
    this.addIntervalBar(interval.enter, interval.exit, y, {
      fill,
      stroke: key === 'a' ? '#2060d0' : '#20a050',
      'data-interval': label,
    });
    this.addMarker(interval.enter, y, 'enter', 'enter');
    this.addMarker(interval.exit, y, 'exit', 'exit');
    this.addEndpointHandle(key, 'enter', y);
    this.addEndpointHandle(key, 'exit', y);
  }

  renderResult() {
    const y = this.rows.result;
    this.addText(16, y + 5, 'out', { 'font-weight': '600' });
    this.resultIntervals().forEach((span) => {
      this.addIntervalBar(span.enter, span.exit, y, {
        fill: '#fff3bf',
        stroke: '#f08c00',
        'data-result-interval': this.operation,
      });
    });
    this.resultEvents().forEach((event) => {
      this.addMarker(event.t, y, event.kind, event.kind, {
        fill: event.flipped ? '#ffd8a8' : '#ffffff',
        stroke: event.flipped ? '#e8590c' : '#111',
        'data-result-marker': event.kind,
        'data-source-shape': event.source,
        'data-normal-flipped': event.flipped ? 'true' : 'false',
      });
    });

    const hit = this.selectedHit();
    if (!hit) {
      this.addText(this.bounds.left, 232, 'No positive-distance hit in the result interval.');
      return;
    }
    const hitX = this.xForT(hit.t);
    this.canvas.add('circle', {
      cx: hitX,
      cy: y,
      r: 12,
      fill: 'none',
      stroke: '#f03e3e',
      'stroke-width': FigurePixelStrokeWidth,
      'data-selected-positive-hit': 'true',
    });
    const normalText = hit.flipped ? ', B normal flipped' : '';
    this.addText(this.bounds.left, 232,
      `first positive hit: t=${hit.t.toFixed(2)} (${hit.kind}${normalText})`);
  }

  render() {
    this.canvas.clear();
    this.renderAxis();
    this.renderSourceRow('a', this.rows.a, '#d9e8ff');
    this.renderSourceRow('b', this.rows.b, '#d3f9d8');
    this.renderResult();
  }
}

((scriptElement) => {
  const figure = new CsgHitIntervals();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
