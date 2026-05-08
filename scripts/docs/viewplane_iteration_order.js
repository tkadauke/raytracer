// Interactive widget for ViewPlane's iteration-strategy docs. It shows the
// order in which different view planes write the same pixel grid, including
// progressive block writes from PointInterlacedViewPlane.

class ViewPlaneIterationOrder {
  constructor() {
    this.cols = 16;
    this.rows = 10;
    this.cell = 24;
    this.captionHeight = 28;
    this.progress = 12;
    this.mode = 'point-interlaced';
    this.modes = [
      { value: 'row-major', label: 'Row-major' },
      { value: 'tiled', label: 'Tiled' },
      { value: 'row-interlaced', label: 'Row interlaced' },
      { value: 'point-interlaced', label: 'Point interlaced' },
      { value: 'row-shuffled', label: 'Row shuffled' },
      { value: 'point-shuffled', label: 'Point shuffled' },
    ];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'viewplane-iteration-widget' });
    this.canvas = new FigureSvg({
      width: this.width(),
      height: this.height(),
      viewBox: `0 0 ${this.width()} ${this.height()}`,
    });

    this.modeControl = new FigureSegmentedControl({
      label: 'mode',
      value: this.mode,
      options: this.modes,
      onChange: (value) => {
        this.mode = value;
        this.render();
      },
    });
    this.progressControl = new FigureSliderControl({
      label: 'progress',
      min: 0,
      max: 100,
      step: 1,
      value: this.progress,
      precision: 0,
      format: value => `${Math.round(value)}%`,
      onChange: (value) => {
        this.progress = value;
        this.render();
      },
    });

    this.widget.addControl(this.modeControl.element());
    this.widget.addControl(this.progressControl.element());
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

  render() {
    const sequence = this.sequenceForMode(this.mode);
    const steps = Math.round(sequence.length * this.progress / 100);
    const cells = this.appliedCells(sequence, steps);

    this.canvas.clear();
    this.renderCells(cells, sequence.length);
    this.renderCaption(steps, sequence.length);
  }

  renderCells(cells, totalSteps) {
    for (let row = 0; row < this.rows; row++) {
      for (let col = 0; col < this.cols; col++) {
        const key = this.key(row, col);
        const order = cells.get(key);
        this.canvas.add('rect', {
          x: col * this.cell,
          y: row * this.cell,
          width: this.cell,
          height: this.cell,
          fill: order === undefined ? '#f8f9fa' : this.colorForOrder(order, totalSteps),
          stroke: '#d0d0d0',
          'stroke-width': FigurePixelGuideStrokeWidth,
          'data-viewplane-cell': key,
          'data-rendered': order === undefined ? '0' : '1',
        });
      }
    }
  }

  renderCaption(steps, totalSteps) {
    const mode = this.modes.find(candidate => candidate.value === this.mode).label;
    const caption = this.canvas.add('text', {
      x: 0,
      y: this.rows * this.cell + 20,
      'font-family': 'sans-serif',
      'font-size': 13,
      fill: '#333',
      'data-viewplane-caption': this.mode,
    });
    caption.textContent = `${mode}: ${steps} / ${totalSteps} iterator steps`;
  }

  appliedCells(sequence, steps) {
    const cells = new Map();
    for (let i = 0; i < steps; i++) {
      const step = sequence[i];
      for (let row = step.row; row < Math.min(this.rows, step.row + step.size); row++) {
        for (let col = step.col; col < Math.min(this.cols, step.col + step.size); col++) {
          cells.set(this.key(row, col), i);
        }
      }
    }
    return cells;
  }

  key(row, col) {
    return `${row},${col}`;
  }

  colorForOrder(order, totalSteps) {
    const t = totalSteps <= 1 ? 1 : order / (totalSteps - 1);
    const mix = (a, b) => Math.round(a + (b - a) * t);
    return `rgb(${mix(13, 255)}, ${mix(110, 193)}, ${mix(253, 7)})`;
  }

  sequenceForMode(mode) {
    if (mode === 'row-major') return this.rowMajorSequence();
    if (mode === 'tiled') return this.tiledSequence();
    if (mode === 'row-interlaced') return this.rowInterlacedSequence();
    if (mode === 'point-interlaced') return this.pointInterlacedSequence();
    if (mode === 'row-shuffled') return this.rowShuffledSequence();
    return this.pointShuffledSequence();
  }

  rowMajorSequence() {
    const sequence = [];
    for (let row = 0; row < this.rows; row++) {
      for (let col = 0; col < this.cols; col++) {
        sequence.push({ row, col, size: 1 });
      }
    }
    return sequence;
  }

  tiledSequence() {
    const sequence = [];
    const tileSize = 4;
    for (let tileRow = 0; tileRow < this.rows; tileRow += tileSize) {
      for (let tileCol = 0; tileCol < this.cols; tileCol += tileSize) {
        for (let row = tileRow; row < Math.min(this.rows, tileRow + tileSize); row++) {
          for (let col = tileCol; col < Math.min(this.cols, tileCol + tileSize); col++) {
            sequence.push({ row, col, size: 1 });
          }
        }
      }
    }
    return sequence;
  }

  rowInterlacedSequence() {
    const sequence = [];
    let rowJump = this.initialPowerOfE(this.rows);
    let offset = 0;
    let row = 0;

    while (row < this.rows) {
      for (let col = 0; col < this.cols; col++) {
        sequence.push({ row, col, size: 1 });
      }

      row += rowJump;
      if (row >= this.rows) {
        if (offset === 1) {
          break;
        } else {
          if (offset === 0) {
            offset = rowJump / 2;
          } else {
            offset /= 2;
            rowJump /= 2;
          }
          row = offset;
        }
      }
    }

    return sequence;
  }

  pointInterlacedSequence() {
    const sequence = [];
    const initialSize = Math.min(
      this.initialPowerOfE(this.cols),
      this.initialPowerOfE(this.rows)
    );
    let pixelSize = initialSize;
    let evenRow = false;
    let row = 0;
    let col = 0;

    while (row < this.rows) {
      sequence.push({ row, col, size: pixelSize });

      col += evenRow ? pixelSize * 2 : pixelSize;
      if (col >= this.cols) {
        row += pixelSize;
        if (row >= this.rows) {
          if (pixelSize === 1) {
            break;
          } else {
            evenRow = true;
            pixelSize /= 2;
            row = 0;
            col = pixelSize;
          }
        } else {
          if (pixelSize !== initialSize) evenRow = !evenRow;
          col = evenRow ? pixelSize : 0;
        }
      }
    }

    return sequence;
  }

  rowShuffledSequence() {
    const rows = this.shuffle(Array.from({ length: this.rows }, (_, row) => row), 9041);
    const sequence = [];
    rows.forEach((row) => {
      for (let col = 0; col < this.cols; col++) {
        sequence.push({ row, col, size: 1 });
      }
    });
    return sequence;
  }

  pointShuffledSequence() {
    const points = [];
    for (let row = 0; row < this.rows; row++) {
      for (let col = 0; col < this.cols; col++) {
        points.push({ row, col, size: 1 });
      }
    }
    return this.shuffle(points, 17021);
  }

  initialPowerOfE(value) {
    return 1 << Math.floor(Math.log(value));
  }

  shuffle(items, seed) {
    const shuffled = items.slice();
    let state = seed;
    for (let i = shuffled.length - 1; i > 0; i--) {
      state = (state * 1664525 + 1013904223) >>> 0;
      const j = state % (i + 1);
      [shuffled[i], shuffled[j]] = [shuffled[j], shuffled[i]];
    }
    return shuffled;
  }
}

((scriptElement) => {
  const figure = new ViewPlaneIterationOrder();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
