// Interactive widget for Sampler / SampleStream documentation.
// Shows one subpixel sample square and the stream dimensions a camera
// pulls from it: pixel jitter, lens position, and shutter time.

class SamplerStreamsWidget {
  constructor() {
    this.sampler = 'jittered';
    this.sampleCount = 16;
    this.mode = 'independent';
    this.width = 720;
    this.height = 330;
    this.margin = 28;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'sampler-streams-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.samplerControl = new FigureSegmentedControl({
      label: 'sampler',
      value: this.sampler,
      options: [
        { label: 'regular', value: 'regular' },
        { label: 'jittered', value: 'jittered' },
        { label: 'random', value: 'random' },
      ],
      onChange: (value) => {
        this.sampler = value;
        this.render();
      },
    });

    this.countControl = new FigureSegmentedControl({
      label: 'samples',
      value: this.sampleCount,
      options: [4, 9, 16, 25].map(count => ({ label: String(count), value: count })),
      onChange: (value) => {
        this.sampleCount = Number(value);
        this.render();
      },
    });

    this.modeControl = new FigureSegmentedControl({
      label: 'dimensions',
      value: this.mode,
      options: [
        { label: 'reuse one set', value: 'reused' },
        { label: 'independent', value: 'independent' },
      ],
      onChange: (value) => {
        this.mode = value;
        this.render();
      },
    });

    this.widget.addControl(this.samplerControl.element());
    this.widget.addControl(this.countControl.element());
    this.widget.addControl(this.modeControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  samplesForSet(setIndex) {
    const n = Math.sqrt(this.sampleCount);
    const samples = [];
    const random = this.randomGenerator(0x9e3779b9 + setIndex * 7919 + this.sampleCount * 31);

    if (this.sampler === 'regular') {
      for (let x = 0; x < n; x++) {
        for (let y = 0; y < n; y++) {
          samples.push({ x: (x + 0.5) / n, y: (y + 0.5) / n });
        }
      }
      return samples;
    }

    if (this.sampler === 'jittered') {
      for (let x = 0; x < n; x++) {
        for (let y = 0; y < n; y++) {
          samples.push({ x: (x + random()) / n, y: (y + random()) / n });
        }
      }
      return samples;
    }

    for (let i = 0; i < this.sampleCount; i++) {
      samples.push({ x: random(), y: random() });
    }
    return samples;
  }

  randomGenerator(seed) {
    let state = seed >>> 0;
    return () => {
      state = (state * 1664525 + 1013904223) >>> 0;
      return state / 0x100000000;
    };
  }

  render() {
    this.canvas.clear();
    this.renderSubpixelPanel();
    this.renderDimensionPanels();
    this.renderLegend();
  }

  renderSubpixelPanel() {
    const samples = this.samplesForSet(0);
    const frame = { x: this.margin, y: 48, size: 236 };

    this.renderSampleSquare({
      frame,
      samples,
      label: 'subpixel sample square',
      dotFill: '#0b7285',
      dataDimension: 'pixel',
    });
  }

  renderDimensionPanels() {
    const dimensions = [
      { label: 'pixel jitter', set: 0, color: '#0b7285', data: 'pixel' },
      { label: 'lens sample', set: 1, color: '#c92a2a', data: 'lens' },
      { label: 'shutter time', set: 2, color: '#5f3dc4', data: 'shutter-time' },
    ];
    const panelWidth = 108;
    const panelHeight = 108;
    const startX = 318;
    const gap = 18;

    dimensions.forEach((dimension, index) => {
      const setIndex = this.mode === 'reused' ? 0 : dimension.set;
      const samples = this.samplesForSet(setIndex);
      this.renderSampleSquare({
        frame: {
          x: startX + index * (panelWidth + gap),
          y: 84,
          size: panelWidth,
          height: panelHeight,
        },
        samples,
        label: dimension.label,
        dotFill: dimension.color,
        dataDimension: dimension.data,
      });

      const source = this.mode === 'reused' ? 'same set' : `set ${setIndex}`;
      this.addText({
        x: startX + index * (panelWidth + gap) + panelWidth / 2,
        y: 222,
        text: source,
        anchor: 'middle',
        size: 13,
        fill: '#555',
      });
    });
  }

  renderSampleSquare({ frame, samples, label, dotFill, dataDimension }) {
    const size = frame.size;
    const x = frame.x;
    const y = frame.y;
    const n = Math.sqrt(this.sampleCount);

    this.addText({
      x: x + size / 2,
      y: y - 14,
      text: label,
      anchor: 'middle',
      size: 14,
      weight: 600,
    });

    this.canvas.add('rect', {
      x,
      y,
      width: size,
      height: size,
      fill: '#ffffff',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      'data-sample-panel': dataDimension,
    });

    if (this.sampler !== 'random') {
      for (let i = 1; i < n; i++) {
        const pos = x + (i / n) * size;
        this.canvas.add('line', {
          x1: pos,
          y1: y,
          x2: pos,
          y2: y + size,
          stroke: '#d6d6d6',
          'stroke-width': FigurePixelGuideStrokeWidth,
          'data-stratum-line': dataDimension,
        });
        this.canvas.add('line', {
          x1: x,
          y1: y + (i / n) * size,
          x2: x + size,
          y2: y + (i / n) * size,
          stroke: '#d6d6d6',
          'stroke-width': FigurePixelGuideStrokeWidth,
          'data-stratum-line': dataDimension,
        });
      }
    }

    samples.forEach((sample, index) => {
      this.canvas.add('circle', {
        cx: x + sample.x * size,
        cy: y + sample.y * size,
        r: size > 180 ? 5 : 3.8,
        fill: dotFill,
        stroke: '#111',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-sample-dot': dataDimension,
        'data-sample-index': index,
      });
    });
  }

  renderLegend() {
    const message = this.mode === 'reused'
      ? 'Every camera dimension reads the exact same 2D pattern.'
      : 'Each camera dimension reads a different pre-baked set.';

    this.addText({
      x: 318,
      y: 270,
      text: message,
      anchor: 'start',
      size: 14,
      fill: '#222',
    });

    this.addText({
      x: 318,
      y: 294,
      text: 'Independent streams keep stratification',
      anchor: 'start',
      size: 13,
      fill: '#555',
    });

    this.addText({
      x: 318,
      y: 314,
      text: 'without correlating lens and time with pixel jitter.',
      anchor: 'start',
      size: 13,
      fill: '#555',
    });
  }

  addText({ x, y, text, anchor = 'start', size = 12, fill = '#111', weight = 400 }) {
    const node = this.canvas.add('text', {
      x,
      y,
      'text-anchor': anchor,
      'font-size': size,
      'font-family': '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
      'font-weight': weight,
      fill,
    });
    node.textContent = text;
    return node;
  }
}

((scriptElement) => {
  const figure = new SamplerStreamsWidget();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
