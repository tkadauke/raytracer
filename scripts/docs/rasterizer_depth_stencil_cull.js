// Interactive widget for Rasterizer depth, stencil, and face-culling state.
//
// The first pass writes a small stencil mask. The second pass draws two
// overlapping triangles through that mask, using the depth buffer to keep the
// nearest fragment and the cull mode to reject front- or back-facing triangles.

class RasterizerDepthStencilTriangle {
  constructor(name, vertices, depth, color) {
    this.name = name;
    this.vertices = vertices;
    this.depth = depth;
    this.color = color;
    this.area = FigureGeometry.edge(vertices[0], vertices[1], vertices[2]);
    this.facing = this.area < 0 ? 'front' : 'back';
  }

  contains(point) {
    return FigureGeometry.pointInTriangle(
      point, this.vertices[0], this.vertices[1], this.vertices[2]);
  }
}

class RasterizerDepthStencilCull {
  constructor() {
    this.cols = 8;
    this.rows = 6;
    this.cell = 34;
    this.panelGap = 28;
    this.titleHeight = 22;
    this.statusHeight = 44;
    this.stencilEnabled = true;
    this.cullMode = 'both';
    this.triangles = [
      new RasterizerDepthStencilTriangle('back triangle', [
        new Vector(6.9, 1.2),
        new Vector(5.8, 5.1),
        new Vector(1.4, 1.5),
      ], 0.72, '#4f83d1'),
      new RasterizerDepthStencilTriangle('front triangle', [
        new Vector(1.0, 4.8),
        new Vector(6.6, 4.1),
        new Vector(2.0, 1.0),
      ], 0.34, '#d34a36'),
    ];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-depth-stencil-widget' });
    this.canvas = new FigureSvg({
      width: this.width(),
      height: this.height(),
      viewBox: `0 0 ${this.width()} ${this.height()}`,
    });

    this.stencilControl = new FigureSegmentedControl({
      label: 'stencil test',
      value: this.stencilEnabled,
      options: [
        { label: 'On', value: true },
        { label: 'Off', value: false },
      ],
      onChange: (value) => {
        this.stencilEnabled = value;
        this.render();
      },
    });
    this.cullControl = new FigureSegmentedControl({
      label: 'cull mode',
      value: this.cullMode,
      options: [
        { label: 'Both', value: 'both' },
        { label: 'Back', value: 'back' },
        { label: 'Front', value: 'front' },
      ],
      onChange: (value) => {
        this.cullMode = value;
        this.render();
      },
    });

    this.widget.addControl(this.stencilControl.element());
    this.widget.addControl(this.cullControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  panelWidth() {
    return this.cols * this.cell;
  }

  panelHeight() {
    return this.titleHeight + this.rows * this.cell;
  }

  width() {
    return this.panelWidth() * 3 + this.panelGap * 2;
  }

  height() {
    return this.panelHeight() + this.statusHeight;
  }

  maskForPixel(x, y) {
    const center = new Vector(x + 0.5, y + 0.5);
    return Math.abs(center.x - 4.0) + Math.abs(center.y - 3.0) <= 2.35;
  }

  culls(triangle, mode = this.cullMode) {
    if (mode === 'both') return false;
    return mode === triangle.facing;
  }

  simulate() {
    const color = [];
    const depth = [];
    const stencil = [];
    const owner = [];
    for (let y = 0; y < this.rows; ++y) {
      color[y] = [];
      depth[y] = [];
      stencil[y] = [];
      owner[y] = [];
      for (let x = 0; x < this.cols; ++x) {
        color[y][x] = '#f4f4f4';
        depth[y][x] = Infinity;
        stencil[y][x] = this.maskForPixel(x, y) ? 1 : 0;
        owner[y][x] = '';
      }
    }

    const culled = [];
    for (const triangle of this.triangles) {
      if (this.culls(triangle)) {
        culled.push(triangle.name);
        continue;
      }

      for (let y = 0; y < this.rows; ++y) {
        for (let x = 0; x < this.cols; ++x) {
          if (this.stencilEnabled && stencil[y][x] !== 1) continue;
          if (!triangle.contains(new Vector(x + 0.5, y + 0.5))) continue;
          if (triangle.depth >= depth[y][x]) continue;

          depth[y][x] = triangle.depth;
          color[y][x] = triangle.color;
          owner[y][x] = triangle.name;
        }
      }
    }

    return { color, depth, stencil, owner, culled };
  }

  depthColor(value) {
    if (!Number.isFinite(value)) return '#f4f4f4';
    const shade = Math.round(230 - value * 145);
    return `rgb(${shade}, ${shade}, ${shade})`;
  }

  render() {
    this.canvas.clear();
    this.canvas.element.setAttribute('data-stencil-enabled', this.stencilEnabled ? '1' : '0');
    this.canvas.element.setAttribute('data-cull-mode', this.cullMode);

    const result = this.simulate();
    const colorX = 0;
    const depthX = this.panelWidth() + this.panelGap;
    const stencilX = (this.panelWidth() + this.panelGap) * 2;

    this.renderCellPanel(colorX, 'framebuffer', result.color, (fill, x, y) => ({
      fill,
      'data-buffer': 'color',
      'data-owner': result.owner[y][x],
    }));
    this.renderTriangleOutlines(colorX, this.cullMode);

    this.renderCellPanel(depthX, 'depth buffer', result.depth, (value) => ({
      fill: this.depthColor(value),
      'data-buffer': 'depth',
      'data-depth': Number.isFinite(value) ? value.toFixed(2) : 'clear',
    }));

    this.renderCellPanel(stencilX, 'stencil mask', result.stencil, (value) => ({
      fill: value ? '#7fc97f' : '#f4f4f4',
      'data-buffer': 'stencil',
      'data-stencil': String(value),
      'data-stencil-tested': this.stencilEnabled && value === 1 ? '1' : '0',
      opacity: this.stencilEnabled || value ? '1' : '0.55',
    }));
    this.renderTriangleOutlines(stencilX, 'both');
    this.renderStatus(result.culled);
  }

  renderCellPanel(x0, title, cells, cellAttrs) {
    const label = this.canvas.add('text', {
      x: x0,
      y: 15,
      'font-family': 'sans-serif',
      'font-size': 14,
      'font-weight': 'bold',
      fill: '#222',
    });
    label.textContent = title;

    for (let y = 0; y < this.rows; ++y) {
      for (let x = 0; x < this.cols; ++x) {
        this.canvas.add('rect', {
          x: x0 + x * this.cell,
          y: this.titleHeight + y * this.cell,
          width: this.cell,
          height: this.cell,
          stroke: '#cfcfcf',
          'stroke-width': FigurePixelGuideStrokeWidth,
          ...cellAttrs(cells[y][x], x, y),
        });
      }
    }
  }

  trianglePoints(triangle, x0) {
    return triangle.vertices
      .map(v => `${x0 + v.x * this.cell},${this.titleHeight + v.y * this.cell}`)
      .join(' ');
  }

  renderTriangleOutlines(x0, mode) {
    for (const triangle of this.triangles) {
      const isCulled = this.culls(triangle, mode);
      const attrs = {
        points: this.trianglePoints(triangle, x0),
        fill: 'none',
        stroke: isCulled ? '#777' : '#111',
        'stroke-width': isCulled ? FigurePixelGuideStrokeWidth : FigurePixelStrokeWidth,
        'data-triangle-facing': triangle.facing,
        'data-triangle-culled': isCulled ? '1' : '0',
      };
      if (isCulled) attrs['stroke-dasharray'] = '7 5';
      this.canvas.add('polygon', attrs);
    }
  }

  renderStatus(culledTriangles) {
    const status = this.canvas.add('text', {
      x: 0,
      y: this.panelHeight() + 30,
      'font-family': 'sans-serif',
      'font-size': 13,
      fill: '#333',
    });
    const stencil = this.stencilEnabled ? 'only where stencil == 1' : 'without stencil';
    const culled = culledTriangles.length ? culledTriangles.join(', ') : 'none';
    status.textContent = `pass 1 writes stencil=1; pass 2 draws ${stencil}; culled: ${culled}`;
  }
}

((scriptElement) => {
  const figure = new RasterizerDepthStencilCull();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
