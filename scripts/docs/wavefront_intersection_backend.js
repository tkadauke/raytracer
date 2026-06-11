// Interactive widget for the Wavefront and path tracing textbook chapter.
// It shows that the optional GPU backend owns only ray-scene intersection;
// wavefront scheduling and CPU shading stay on the same renderer path.

class WavefrontIntersectionBackendWidget {
  constructor() {
    this.backend = 'metal';
    this.query = 'closest';
    this.boundary = 'hybrid';
    this.width = 760;
    this.height = 370;
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

    this.boundaryControl = new FigureSegmentedControl({
      label: 'boundary',
      value: this.boundary,
      options: [
        { label: 'hybrid now', value: 'hybrid' },
        { label: 'resident target', value: 'resident' },
      ],
      onChange: (value) => {
        this.boundary = value;
        this.render();
      },
    });

    this.widget.addControl(this.backendControl.element());
    this.widget.addControl(this.queryControl.element());
    this.widget.addControl(this.boundaryControl.element());
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
    const title = this.boundary === 'hybrid'
      ? 'Intersection backend: the scheduler asks, the backend answers'
      : 'Resident-frontier target: fewer host/device crossings';
    this.text(28, 34, title, {
      size: 18,
      weight: 700,
    });
  }

  schedulerPanel() {
    this.panel(38, 74, 188, 156, 'CPU scheduler', '#e7f5ff', '#1864ab');
    this.badge(66, 128, 'depth frontier', '#d0ebff', '#1864ab');
    this.badge(66, 166, 'path states', '#d0ebff', '#1864ab');
    if (this.boundary === 'resident') {
      this.badge(66, 198, 'dispatch policy', '#d0ebff', '#1864ab');
    }
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
    this.panel(286, 74, 202, 156, labels[this.backend], color[0], color[1]);
    this.badge(322, 128, this.query === 'closest' ? 'closest-hit batch' : 'bounded any-hit batch',
      '#ffffff', color[1], 126);
    this.badge(322, 166, 'packed ray ABI', '#ffffff', color[1], 126);
    if (this.boundary === 'resident') {
      this.badge(322, 198, 'frontier cache', '#ffffff', color[1], 126);
    }
  }

  resultPanel() {
    this.panel(548, 74, 174, 156, 'CPU integrator', '#fff9db', '#a16207');
    this.badge(572, 128, this.query === 'closest' ? 'hit records' : 'occlusion bits',
      '#fff3bf', '#a16207', 126);
    this.badge(572, 166, 'shade / continue', '#fff3bf', '#a16207', 126);
    if (this.boundary === 'resident') {
      this.badge(572, 198, 'trace counters', '#fff3bf', '#a16207', 126);
    }
  }

  flow() {
    const rayLabel = this.query === 'closest' ? 'camera/path rays' : 'shadow rays';
    const resultLabel = this.query === 'closest' ? 'primitive + t + normal + uv' : 'visible / blocked';

    this.arrow(228, 136, 284, 136, '#1864ab');
    this.text(236, 124, this.boundary === 'hybrid' ? `upload ${rayLabel}` : `launch ${rayLabel}`, {
      size: 11,
      fill: '#1864ab',
    });

    this.arrow(490, 136, 546, 136, '#087f5b');
    this.text(498, 124, this.boundary === 'hybrid' ? `readback ${resultLabel}` : resultLabel, {
      size: 11,
      fill: '#087f5b',
    });

    this.arrow(546, 184, 490, 184, '#a16207');
    this.text(498, 248, this.boundary === 'hybrid'
      ? 'Each query crosses the host/device boundary.'
      : 'Resident frontiers keep reusable ray state on device between query phases.',
    { size: 12, fill: '#a16207' });

    if (this.boundary === 'resident') {
      this.arrow(388, 232, 388, 252, '#087f5b');
      this.badge(316, 258, 'compact active rays', '#e6fcf5', '#087f5b', 144);
      this.arrow(388, 256, 388, 232, '#087f5b');
    }
  }

  note() {
    const platformNote = {
      cpu: 'CPU supports the full scene. GPU requests fall back here when a scene or platform is unsupported.',
      metal: 'Metal can execute the current basic closest-hit and any-hit kernels for eligible packed scenes.',
      vulkan: 'Vulkan can execute the current basic closest-hit and any-hit kernels for eligible packed scenes.',
    };
    this.text(44, 300, platformNote[this.backend], { size: 13, fill: '#343a40' });
    const ownership = this.boundary === 'hybrid'
      ? 'Today every backend query uploads rays and reads back hit/occlusion results before CPU shading continues.'
      : 'The future target is to reuse device-side frontiers and compaction state; CPU shading remains the semantic owner.';
    this.text(44, 326, ownership, { size: 13, fill: '#343a40' });
    this.text(44, 350,
      'Materials, BSDF sampling, direct lighting, denoising, tonemapping, and graph scheduling stay outside the backend.',
      { size: 12, fill: '#343a40' });
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
