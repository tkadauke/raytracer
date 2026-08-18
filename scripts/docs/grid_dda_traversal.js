// Interactive widget for `render::Grid`'s uniform-grid 3D-DDA traversal.
// Shows one 2D slice of the grid: the ray enters the grid's bounding box,
// advances to the nearest cell boundary, and visits only cells along the ray.

class GridDDATraversal {
  constructor() {
    this.width = 560;
    this.height = 360;
    this.gridSize = 288;
    this.gridOffset = { x: 38, y: 34 };
    this.density = 8;
    this.maxSteps = 28;
    this.handleRadius = 8;
    this.origin = { x: 0.35, y: 5.8 };
    this.target = { x: 7.35, y: 2.95 };
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'grid-dda-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.densityControl = new FigureSliderControl({
      label: 'grid density',
      min: 4,
      max: 14,
      step: 1,
      value: this.density,
      precision: 0,
      format: value => `${value} x ${value}`,
      onChange: (value) => {
        this.density = Math.round(value);
        this.constrainRayToGrid();
        this.render();
      },
    });

    this.widget.addControl(this.densityControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  cellSize() {
    return this.gridSize / this.density;
  }

  handleInsetCells() {
    return this.handleRadius / this.cellSize();
  }

  clampGridPoint(point) {
    const inset = this.handleInsetCells();
    return {
      x: FigureMath.clamp(point.x, inset, this.density - inset),
      y: FigureMath.clamp(point.y, inset, this.density - inset),
    };
  }

  constrainRayToGrid() {
    this.origin = this.clampGridPoint(this.origin);
    this.target = this.clampGridPoint(this.target);
  }

  direction() {
    const dx = this.target.x - this.origin.x;
    const dy = this.target.y - this.origin.y;
    const len = Math.hypot(dx, dy);
    if (len < 1e-6) return { x: 1, y: 0 };
    return { x: dx / len, y: dy / len };
  }

  intersectGrid() {
    const dir = this.direction();
    const bounds = { min: 0, max: this.density };
    let tMin = -Infinity;
    let tMax = Infinity;

    for (const axis of ['x', 'y']) {
      if (Math.abs(dir[axis]) < 1e-9) {
        if (this.origin[axis] < bounds.min || this.origin[axis] > bounds.max) {
          return null;
        }
        continue;
      }

      const t0 = (bounds.min - this.origin[axis]) / dir[axis];
      const t1 = (bounds.max - this.origin[axis]) / dir[axis];
      tMin = Math.max(tMin, Math.min(t0, t1));
      tMax = Math.min(tMax, Math.max(t0, t1));
    }

    if (tMax < Math.max(0, tMin)) return null;
    return { tEnter: Math.max(0, tMin), tExit: tMax };
  }

  traversal() {
    const hit = this.intersectGrid();
    if (!hit) return { entry: null, steps: [] };

    const dir = this.direction();
    const n = this.density;
    const entry = {
      x: this.origin.x + dir.x * hit.tEnter,
      y: this.origin.y + dir.y * hit.tEnter,
    };
    let cellX = FigureMath.clamp(Math.floor(entry.x), 0, n - 1);
    let cellY = FigureMath.clamp(Math.floor(entry.y), 0, n - 1);
    const stepX = dir.x > 0 ? 1 : -1;
    const stepY = dir.y > 0 ? 1 : -1;
    const deltaX = Math.abs(dir.x) < 1e-9 ? Infinity : Math.abs(1 / dir.x);
    const deltaY = Math.abs(dir.y) < 1e-9 ? Infinity : Math.abs(1 / dir.y);
    let nextX = Math.abs(dir.x) < 1e-9
      ? Infinity
      : (((dir.x > 0 ? cellX + 1 : cellX) - this.origin.x) / dir.x);
    let nextY = Math.abs(dir.y) < 1e-9
      ? Infinity
      : (((dir.y > 0 ? cellY + 1 : cellY) - this.origin.y) / dir.y);
    const steps = [];

    for (let i = 0; i < this.maxSteps; i++) {
      if (cellX < 0 || cellX >= n || cellY < 0 || cellY >= n) break;
      steps.push({
        x: cellX,
        y: cellY,
        tNextX: nextX,
        tNextY: nextY,
        nextAxis: nextX < nextY ? 'x' : 'y',
      });

      if (Math.min(nextX, nextY) > hit.tExit) break;
      if (nextX < nextY) {
        cellX += stepX;
        nextX += deltaX;
      } else {
        cellY += stepY;
        nextY += deltaY;
      }
    }

    const exit = {
      x: this.origin.x + dir.x * hit.tExit,
      y: this.origin.y + dir.y * hit.tExit,
    };

    return { entry, exit, steps };
  }

  toSvgPoint(point) {
    const cell = this.cellSize();
    return {
      x: this.gridOffset.x + point.x * cell,
      y: this.gridOffset.y + point.y * cell,
    };
  }

  fromSvgPoint(point) {
    const cell = this.cellSize();
    return this.clampGridPoint({
      x: (point.x - this.gridOffset.x) / cell,
      y: (point.y - this.gridOffset.y) / cell,
    });
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

  addText(x, y, text, attrs = {}) {
    const label = this.canvas.add('text', {
      x,
      y,
      'font-family': 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace',
      'font-size': 13,
      fill: '#222',
      ...attrs,
    });
    label.textContent = text;
    return label;
  }

  render() {
    this.canvas.clear();
    this.addArrowMarker();
    const result = this.traversal();
    this.renderGrid();
    this.renderPrimitiveDistribution();
    this.renderVisitedCells(result.steps);
    this.renderCurrentCell(result.steps[0]);
    this.renderRay(result);
    this.renderLabels(result);
    this.renderHandles();
  }

  addArrowMarker() {
    const defs = createSvgElement('defs');
    const marker = createSvgElement('marker', {
      id: 'grid-dda-arrow',
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

  renderGrid() {
    const cell = this.cellSize();
    const n = this.density;
    for (let i = 0; i <= n; i++) {
      const x = this.gridOffset.x + i * cell;
      const y = this.gridOffset.y + i * cell;
      this.addLine({ x, y: this.gridOffset.y }, { x, y: this.gridOffset.y + this.gridSize }, {
        stroke: i === 0 || i === n ? '#555' : '#d0d0d0',
        'stroke-width': i === 0 || i === n ? FigurePixelStrokeWidth : FigurePixelGuideStrokeWidth,
      });
      this.addLine({ x: this.gridOffset.x, y }, { x: this.gridOffset.x + this.gridSize, y }, {
        stroke: i === 0 || i === n ? '#555' : '#d0d0d0',
        'stroke-width': i === 0 || i === n ? FigurePixelStrokeWidth : FigurePixelGuideStrokeWidth,
      });
    }
  }

  renderPrimitiveDistribution() {
    const n = this.density;
    const points = [];
    for (let y = 0; y < n; y++) {
      for (let x = 0; x < n; x++) {
        if ((x * 7 + y * 11) % 9 === 0) {
          points.push({ x: x + 0.35 + ((x + y) % 3) * 0.12, y: y + 0.35 });
        }
      }
    }

    for (const point of points) {
      const svg = this.toSvgPoint(point);
      this.canvas.add('circle', {
        cx: svg.x,
        cy: svg.y,
        r: 3.5,
        fill: '#495057',
        stroke: '#fff',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-grid-primitive': 'even',
      });
    }
  }

  renderVisitedCells(steps) {
    const cell = this.cellSize();
    steps.forEach((step, index) => {
      this.canvas.add('rect', {
        x: this.gridOffset.x + step.x * cell,
        y: this.gridOffset.y + step.y * cell,
        width: cell,
        height: cell,
        fill: '#4dabf7',
        'fill-opacity': 0.18 + Math.min(0.28, index * 0.015),
        stroke: '#1971c2',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-grid-dda-cell': 'visited',
        'data-step-index': index,
      });
    });
  }

  renderCurrentCell(step) {
    if (!step) return;
    const cell = this.cellSize();
    this.canvas.add('rect', {
      x: this.gridOffset.x + step.x * cell + 3,
      y: this.gridOffset.y + step.y * cell + 3,
      width: cell - 6,
      height: cell - 6,
      fill: '#ffd43b',
      'fill-opacity': 0.35,
      stroke: '#e67700',
      'stroke-width': FigurePixelStrokeWidth,
      'data-grid-dda-cell': 'current',
    });
  }

  renderRay(result) {
    const dir = this.direction();
    const rayEnd = result.exit
      ? this.clampGridPoint({
        x: result.exit.x - dir.x * this.handleInsetCells(),
        y: result.exit.y - dir.y * this.handleInsetCells(),
      })
      : this.target;
    const start = this.toSvgPoint(this.clampGridPoint(result.entry || this.origin));
    const end = this.toSvgPoint(rayEnd);
    this.addLine(start, end, {
      stroke: '#d6336c',
      'marker-end': 'url(#grid-dda-arrow)',
      'data-grid-dda-ray': 'segment',
    });

    if (result.entry) {
      const p = this.toSvgPoint(this.clampGridPoint(result.entry));
      this.canvas.add('circle', {
        cx: p.x,
        cy: p.y,
        r: 6,
        fill: '#fff',
        stroke: '#d6336c',
        'stroke-width': FigurePixelStrokeWidth,
        'data-grid-dda-entry': '1',
      });
      this.addText(p.x + 8, p.y - 8, 'entry', {
        fill: '#a61e4d',
        'font-size': 12,
      });
    }
  }

  renderLabels(result) {
    const panelX = this.gridOffset.x + this.gridSize + 28;
    this.addText(panelX, 54, 'uniform grid traversal');
    this.addText(panelX, 80, `${this.density * this.density} cells in this slice`);
    this.addText(panelX, 112, `visited cells: ${result.steps.length}`, {
      fill: '#1864ab',
    });

    const current = result.steps[0];
    if (!current) {
      this.addText(panelX, 144, 'ray misses the grid', { fill: '#a61e4d' });
      return;
    }

    this.addText(panelX, 144, `current cell: (${current.x}, ${current.y})`, {
      fill: '#e67700',
    });
    this.addText(panelX, 176, `t_next x = ${this.formatT(current.tNextX)}`, {
      fill: current.nextAxis === 'x' ? '#d6336c' : '#495057',
    });
    this.addText(panelX, 202, `t_next y = ${this.formatT(current.tNextY)}`, {
      fill: current.nextAxis === 'y' ? '#d6336c' : '#495057',
    });
    this.addText(panelX, 234, `step ${current.nextAxis} first`, {
      fill: '#d6336c',
    });
    this.addText(panelX, 284, 'even spread keeps', { fill: '#495057' });
    this.addText(panelX, 304, 'cell lists short', { fill: '#495057' });
  }

  formatT(value) {
    if (!Number.isFinite(value)) return 'inf';
    return value.toFixed(2);
  }

  renderHandles() {
    const originHandle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.toSvgPoint(this.origin),
      radius: this.handleRadius,
      attrs: {
        fill: '#d6336c',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'ray-origin',
      },
      onDrag: (point) => {
        this.origin = this.fromSvgPoint(point);
        this.render();
      },
    });
    this.canvas.append(originHandle.element());

    const targetHandle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.toSvgPoint(this.target),
      radius: this.handleRadius,
      attrs: {
        fill: '#fff',
        stroke: '#d6336c',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'ray-direction',
      },
      onDrag: (point) => {
        this.target = this.fromSvgPoint(point);
        this.render();
      },
    });
    this.canvas.append(targetHandle.element());
  }
}

((scriptElement) => {
  const figure = new GridDDATraversal();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
