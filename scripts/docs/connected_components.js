// Interactive widget for textbook chapter 23 — Blob analysis and silhouettes.
// Animates the BFS flood-fill that turns a target-coloured raster into a
// list of connected components. The step slider advances the BFS one
// pixel-pop at a time; each component gets its own colour as it's
// discovered.

class ConnectedComponents {
  constructor() {
    this.cols = 18;
    this.rows = 11;
    this.cell = 26;
    this.captionHeight = 32;

    // 0 = background, 1 = target colour (the BFS seed-set).
    // Hand-crafted to show three disconnected components.
    const T = 1, _ = 0;
    this.raster = [
      [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
      [_, T, T, T, _, _, _, _, _, _, T, _, _, _, T, T, T, _],
      [_, T, _, _, _, _, _, _, T, _, T, T, _, _, T, _, T, _],
      [_, T, _, _, _, _, _, _, _, _, _, T, _, _, T, T, T, _],
      [_, T, T, T, _, _, _, T, T, _, T, T, _, _, _, _, _, _],
      [_, _, _, T, _, _, _, T, _, _, _, T, _, T, T, T, _, _],
      [_, _, _, T, _, _, _, T, T, _, _, _, _, T, _, T, _, _],
      [_, T, T, T, _, _, _, _, _, _, _, _, _, T, T, T, _, _],
      [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
      [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
      [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
    ];

    // Pick component colours that read well on white.
    this.componentColors = [
      '#1f77b4', // blue
      '#ff7f0e', // orange
      '#2ca02c', // green
      '#d62728', // red
      '#9467bd', // purple
      '#8c564b', // brown
    ];
    this.targetColor = '#bbbbbb';
    this.bgColor = '#fafafa';

    // Pre-compute the full BFS trace at construction time so the slider
    // just samples the prefix.
    this.trace = this.computeTrace();
    this.step = this.trace.steps.length;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'connected-components-widget' });
    this.canvas = new FigureSvg({
      width: this.width(),
      height: this.height(),
      viewBox: `0 0 ${this.width()} ${this.height()}`,
    });

    this.stepControl = new FigureSliderControl({
      label: 'BFS steps',
      min: 0,
      max: this.trace.steps.length,
      step: 1,
      value: this.step,
      precision: 0,
      onChange: (value) => {
        this.step = Math.round(value);
        this.render();
      },
    });

    this.widget.addControl(this.stepControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  width() {
    return this.cols * this.cell;
  }

  height() {
    return this.rows * this.cell + this.captionHeight;
  }

  // Run a 4-connectivity BFS over the raster and emit one trace step per
  // pixel popped from the queue. Each step records the (row, col) and the
  // component-id the cell belongs to.
  computeTrace() {
    const visited = Array.from({ length: this.rows }, () => Array(this.cols).fill(false));
    const componentOf = Array.from({ length: this.rows }, () => Array(this.cols).fill(-1));
    const steps = [];
    let nextComponentId = 0;

    for (let r = 0; r < this.rows; r++) {
      for (let c = 0; c < this.cols; c++) {
        if (this.raster[r][c] !== 1 || visited[r][c]) continue;
        const componentId = nextComponentId++;
        const queue = [[r, c]];
        visited[r][c] = true;
        while (queue.length > 0) {
          const [pr, pc] = queue.shift();
          componentOf[pr][pc] = componentId;
          steps.push({ row: pr, col: pc, componentId });
          for (const [dr, dc] of [[-1, 0], [1, 0], [0, -1], [0, 1]]) {
            const nr = pr + dr;
            const nc = pc + dc;
            if (nr < 0 || nr >= this.rows || nc < 0 || nc >= this.cols) continue;
            if (visited[nr][nc] || this.raster[nr][nc] !== 1) continue;
            visited[nr][nc] = true;
            queue.push([nr, nc]);
          }
        }
      }
    }

    return { steps, componentCount: nextComponentId };
  }

  render() {
    this.canvas.clear();
    this.renderGrid();
    this.renderCaption();
  }

  renderGrid() {
    // Build a per-cell component map for the current step prefix.
    const componentAt = new Map();
    for (let i = 0; i < this.step; i++) {
      const s = this.trace.steps[i];
      componentAt.set(this.key(s.row, s.col), s.componentId);
    }

    for (let r = 0; r < this.rows; r++) {
      for (let c = 0; c < this.cols; c++) {
        const isTarget = this.raster[r][c] === 1;
        const componentId = componentAt.get(this.key(r, c));
        let fill;
        if (componentId !== undefined) {
          fill = this.componentColors[componentId % this.componentColors.length];
        } else if (isTarget) {
          fill = this.targetColor;
        } else {
          fill = this.bgColor;
        }
        this.canvas.add('rect', {
          x: c * this.cell,
          y: r * this.cell,
          width: this.cell,
          height: this.cell,
          fill,
          stroke: '#e5e5e5',
          'stroke-width': FigurePixelGuideStrokeWidth,
        });
      }
    }
  }

  renderCaption() {
    const total = this.trace.steps.length;
    const componentCount = this.trace.componentCount;
    const visitedComponents = new Set();
    for (let i = 0; i < this.step; i++) {
      visitedComponents.add(this.trace.steps[i].componentId);
    }
    const text = `${this.step} / ${total} cells visited, ${visitedComponents.size} / ${componentCount} components discovered`;
    const caption = this.canvas.add('text', {
      x: 4,
      y: this.rows * this.cell + 22,
      'font-family': 'sans-serif',
      'font-size': 13,
      fill: '#333',
    });
    caption.textContent = text;
  }

  key(row, col) {
    return `${row},${col}`;
  }
}

((scriptElement) => {
  const figure = new ConnectedComponents();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
