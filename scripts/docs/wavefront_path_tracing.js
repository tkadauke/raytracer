// Interactive widget for the Wavefront and path tracing textbook chapter.
// It contrasts transport semantics with scheduling: Whitted recursion branches
// through material callbacks, scalar path tracing walks sample paths, and
// wavefront path tracing batches the same path states by depth.

class WavefrontPathTracingWidget {
  constructor() {
    this.mode = 'wavefront';
    this.depth = 3;
    this.width = 760;
    this.height = 390;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'wavefront-path-tracing-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.modeControl = new FigureSegmentedControl({
      label: 'schedule',
      value: this.mode,
      options: [
        { label: 'Whitted', value: 'whitted' },
        { label: 'scalar path tracer', value: 'scalar' },
        { label: 'wavefront', value: 'wavefront' },
      ],
      onChange: (value) => {
        this.mode = value;
        this.render();
      },
    });

    this.depthControl = new FigureSegmentedControl({
      label: 'bounces',
      value: this.depth,
      options: [1, 2, 3, 4].map(value => ({ label: String(value), value })),
      onChange: (value) => {
        this.depth = Number(value);
        this.render();
      },
    });

    this.widget.addControl(this.modeControl.element());
    this.widget.addControl(this.depthControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  render() {
    this.canvas.clear();
    this.addTitle();

    if (this.mode === 'whitted') {
      this.renderWhitted();
    } else if (this.mode === 'scalar') {
      this.renderScalarPathTracer();
    } else {
      this.renderWavefront();
    }
  }

  addTitle() {
    const titles = {
      whitted: 'Recursive Whitted ray tracing: materials call back into rayColor()',
      scalar: 'Scalar path tracing: one path state walks through bounces',
      wavefront: 'Wavefront path tracing: path states batch by depth frontier',
    };
    this.text(24, 30, titles[this.mode], { size: 18, weight: 700 });
  }

  renderWhitted() {
    const x0 = 58;
    const y0 = 74;
    const gapX = 138;
    const gapY = 54;
    const maxDepth = this.depth + 1;

    for (let d = 0; d < maxDepth; d++) {
      const x = x0 + d * gapX;
      const y = y0 + d * gapY;
      this.node(x, y, d === 0 ? 'primary ray' : `rayColor d${d}`, '#d0ebff', '#1864ab');
      if (d > 0) {
        this.arrow(x - gapX + 100, y - gapY + 22, x, y + 22, '#1864ab');
      }

      if (d < maxDepth - 1) {
        this.smallNode(x + 22, y + 72, 'shadow ray', '#fff3bf', '#a16207', 98);
        this.arrow(x + 50, y + 44, x + 72, y + 72, '#a16207');
        this.smallNode(x + 90, y + 104, 'reflect ray', '#e7f5ff', '#0b7285', 98);
        this.arrow(x + 78, y + 44, x + 118, y + 104, '#0b7285');
      }
    }

    this.note(44, 330, 'Each hit can call the renderer again. The call stack owns branching.');
  }

  renderScalarPathTracer() {
    const x0 = 64;
    const y0 = 82;
    const legendX = 628;
    const nodeWidth = 96;
    const colGap = Math.min(132, (legendX - x0 - nodeWidth - 18) / Math.max(this.depth, 1));
    const rowGap = 58;
    const sampleRows = 4;

    for (let row = 0; row < sampleRows; row++) {
      const y = y0 + row * rowGap;
      this.text(24, y + 25, `s${row}`, { size: 13, fill: '#555' });
      for (let d = 0; d <= this.depth; d++) {
        const x = x0 + d * colGap;
        const active = d <= this.depth - (row === 3 ? 1 : 0);
        const fill = active ? '#e6fcf5' : '#f1f3f5';
        const stroke = active ? '#087f5b' : '#adb5bd';
        this.node(x, y, d === 0 ? 'camera' : `bounce ${d}`, fill, stroke, nodeWidth);
        if (d > 0) {
          this.arrow(x - colGap + nodeWidth, y + 22, x, y + 22,
            active ? '#087f5b' : '#adb5bd');
        }
      }
    }

    this.smallNode(legendX, 90, 'sample light', '#fff3bf', '#a16207', 116);
    this.smallNode(legendX, 140, 'sample BSDF', '#e6fcf5', '#087f5b', 116);
    this.note(44, 340, 'The integrator owns recursion. Materials expose transport; unsupported materials terminate.');
  }

  renderWavefront() {
    const x0 = 70;
    const y0 = 86;
    const colGap = 142;
    const columnHeight = 214;
    const rays = [
      ['p0', 'p1', 'p2', 'p3', 'p4'],
      ['p0', 'p1', 'p3', 'p4'],
      ['p1', 'p3', 'p4'],
      ['p3', 'p4'],
      ['p4'],
    ];

    for (let d = 0; d <= this.depth; d++) {
      const x = x0 + d * colGap;
      this.canvas.panel({ x: x - 24, y: y0 - 26, width: 110, height: columnHeight },
        `depth ${d}`, {
          fill: d === 0 ? '#e7f5ff' : '#ffffff',
          stroke: '#495057',
          rx: 4,
        });

      const labels = rays[d] || [];
      labels.forEach((label, index) => {
        const cy = y0 + 34 + index * 28;
        this.circleNode(x + 30, cy, label, '#d3f9d8', '#2b8a3e');
      });

      this.smallNode(x - 5, y0 + 170, 'intersect', '#e7f5ff', '#1864ab', 84);
      this.smallNode(x - 5, y0 + 216, 'shade', '#fff3bf', '#a16207', 84);

      if (d < this.depth) {
        this.arrow(x + 86, y0 + 48, x + colGap - 24, y0 + 48, '#2b8a3e');
      }
    }

    this.note(44, 354, 'Same path states, different schedule: each depth frontier can use packet traversal and shared diagnostics.');
  }

  node(x, y, label, fill, stroke, width = 100) {
    this.canvas.add('rect', {
      x,
      y,
      width,
      height: 44,
      rx: 5,
      fill,
      stroke,
      'stroke-width': FigurePixelStrokeWidth,
    });
    this.text(x + width / 2, y + 27, label, {
      size: 13,
      anchor: 'middle',
      fill: '#111',
      weight: 600,
    });
  }

  smallNode(x, y, label, fill, stroke, width = 86) {
    this.canvas.add('rect', {
      x,
      y,
      width,
      height: 28,
      rx: 4,
      fill,
      stroke,
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.text(x + width / 2, y + 19, label, {
      size: 12,
      anchor: 'middle',
      fill: '#111',
    });
  }

  circleNode(cx, cy, label, fill, stroke) {
    this.canvas.add('circle', {
      cx,
      cy,
      r: 12,
      fill,
      stroke,
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.text(cx, cy + 4, label, {
      size: 10,
      anchor: 'middle',
      fill: '#111',
      weight: 700,
    });
  }

  arrow(x1, y1, x2, y2, color) {
    this.canvas.arrow(new Vector(x1, y1), new Vector(x2, y2), {
      stroke: color,
      'stroke-width': FigurePixelGuideStrokeWidth,
      markerId: `wavefront-path-tracing-arrow-${color.replace('#', '')}`,
      markerColor: color,
    });
  }

  note(x, y, text) {
    this.text(x, y, text, { size: 14, fill: '#343a40' });
  }

  text(x, y, text, { size = 12, fill = '#111', anchor = 'start', weight = 400 } = {}) {
    return this.canvas.text(x, y, text, {
      'font-size': size,
      'font-weight': weight,
      'text-anchor': anchor,
      fill,
    });
  }
}

((scriptElement) => {
  const figure = new WavefrontPathTracingWidget();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
