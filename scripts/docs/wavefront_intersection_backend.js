// Interactive widget for the Wavefront and path tracing textbook chapter.
// It shows that the optional GPU backend owns only ray-scene intersection;
// wavefront scheduling and CPU shading stay on the same renderer path.

class WavefrontIntersectionBackendWidget {
  constructor() {
    this.backend = 'metal';
    this.query = 'closest';
    this.width = 760;
    this.height = 330;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'wavefront-intersection-backend-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.backendControl = new FigureSegmentedControl({
      label: 'backend',
      value: this.backend,
      options: [
        { label: 'CPU', value: 'cpu' },
        { label: 'Metal', value: 'metal' },
        { label: 'Vulkan', value: 'vulkan' },
      ],
      onChange: (value) => {
        this.backend = value;
        this.render();
      },
    });

    this.queryControl = new FigureSegmentedControl({
      label: 'query',
      value: this.query,
      options: [
        { label: 'closest hit', value: 'closest' },
        { label: 'any hit', value: 'any' },
      ],
      onChange: (value) => {
        this.query = value;
        this.render();
      },
    });

    this.widget.addControl(this.backendControl.element());
    this.widget.addControl(this.queryControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  render() {
    this.canvas.clear();
    this.title();
    this.schedulerPanel();
    this.backendPanel();
    this.resultPanel();
    this.flow();
    this.note();
  }

  title() {
    this.text(28, 34, 'Intersection backend: the scheduler asks, the backend answers', {
      size: 18,
      weight: 700,
    });
  }

  schedulerPanel() {
    this.panel(44, 74, 190, 142, 'wavefront scheduler', '#e7f5ff', '#1864ab');
    this.badge(76, 128, 'depth frontier', '#d0ebff', '#1864ab');
    this.badge(76, 166, 'path states', '#d0ebff', '#1864ab');
  }

  backendPanel() {
    const colors = {
      cpu: ['#f1f3f5', '#495057'],
      metal: ['#e6fcf5', '#087f5b'],
      vulkan: ['#fff3bf', '#a16207'],
    };
    const labels = {
      cpu: 'CPU Scene/BVH',
      metal: 'Metal compute',
      vulkan: 'Vulkan compute',
    };
    const color = colors[this.backend];
    this.panel(286, 74, 190, 142, labels[this.backend], color[0], color[1]);
    this.badge(318, 128, this.query === 'closest' ? 'closest-hit batch' : 'bounded any-hit batch',
      '#ffffff', color[1], 126);
    this.badge(318, 166, 'packed ray ABI', '#ffffff', color[1], 126);
  }

  resultPanel() {
    this.panel(528, 74, 190, 142, 'CPU integrator', '#fff9db', '#a16207');
    this.badge(560, 128, this.query === 'closest' ? 'hit records' : 'occlusion bits',
      '#fff3bf', '#a16207', 126);
    this.badge(560, 166, 'shade / continue', '#fff3bf', '#a16207', 126);
  }

  flow() {
    const rayLabel = this.query === 'closest' ? 'camera/path rays' : 'shadow rays';
    const resultLabel = this.query === 'closest' ? 'primitive + t + normal + uv' : 'visible / blocked';

    this.arrow(236, 136, 284, 136, '#1864ab');
    this.text(242, 124, rayLabel, { size: 11, fill: '#1864ab' });

    this.arrow(476, 136, 526, 136, '#087f5b');
    this.text(484, 124, resultLabel, { size: 11, fill: '#087f5b' });

    this.arrow(526, 184, 476, 184, '#a16207');
    this.text(486, 204, 'new work stays CPU-owned', { size: 11, fill: '#a16207' });
  }

  note() {
    const platformNote = {
      cpu: 'CPU supports the full scene. GPU requests fall back here when a scene or platform is unsupported.',
      metal: 'Metal can execute the current basic closest-hit and any-hit kernels for eligible packed scenes.',
      vulkan: 'Vulkan can execute the current basic closest-hit and any-hit kernels for eligible packed scenes.',
    };
    this.text(44, 266, platformNote[this.backend], { size: 13, fill: '#343a40' });
    this.text(44, 292,
      'Materials, BSDF sampling, direct lighting, denoising, tonemapping, and graph scheduling stay outside the backend.',
      { size: 13, fill: '#343a40' });
  }

  panel(x, y, width, height, title, fill, stroke) {
    this.canvas.panel({ x, y, width, height }, title, {
      fill,
      stroke,
      rx: 6,
    });
  }

  badge(x, y, label, fill, stroke, width = 116) {
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

  arrow(x1, y1, x2, y2, color) {
    this.canvas.arrow(new Vector(x1, y1), new Vector(x2, y2), {
      stroke: color,
      'stroke-width': FigurePixelGuideStrokeWidth,
      markerId: `wavefront-intersection-backend-arrow-${color.replace('#', '')}`,
      markerColor: color,
    });
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
  const figure = new WavefrontIntersectionBackendWidget();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
