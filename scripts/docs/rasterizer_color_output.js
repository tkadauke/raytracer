// Interactive widget for the Rasterizer color-output stage.
//
// A shaded source color has already passed coverage, stencil, and depth. This
// stage optionally blends it with the destination framebuffer color, then masks
// RGB channels before writing the result back.

class RasterizerColorOutputWidget {
  constructor() {
    this.width = 760;
    this.height = 410;
    this.sourceName = 'red';
    this.destinationName = 'slate';
    this.blendEnabled = true;
    this.sourceFactor = 'ConstantAlpha';
    this.destinationFactor = 'OneMinusConstantAlpha';
    this.blendOp = 'Add';
    this.constantAlpha = 0.45;
    this.writeMask = { r: true, g: true, b: true };
    this.colors = {
      red: { r: 0.95, g: 0.16, b: 0.10 },
      green: { r: 0.10, g: 0.70, b: 0.28 },
      blue: { r: 0.16, g: 0.34, b: 0.90 },
      gold: { r: 0.86, g: 0.58, b: 0.12 },
      slate: { r: 0.18, g: 0.24, b: 0.32 },
      gray: { r: 0.45, g: 0.45, b: 0.45 },
    };
    this.constantColor = { r: 0.35, g: 0.55, b: 0.80 };
    this.factorOptions = [
      'Zero',
      'One',
      'SourceColor',
      'OneMinusSourceColor',
      'DestinationColor',
      'OneMinusDestinationColor',
      'ConstantColor',
      'OneMinusConstantColor',
      'ConstantAlpha',
      'OneMinusConstantAlpha',
    ];
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-color-output-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.widget.addControl(new FigureSegmentedControl({
      label: 'source',
      value: this.sourceName,
      options: [
        { label: 'Red', value: 'red' },
        { label: 'Green', value: 'green' },
        { label: 'Blue', value: 'blue' },
        { label: 'Gold', value: 'gold' },
      ],
      onChange: (value) => {
        this.sourceName = value;
        this.render();
      },
    }).element());

    this.widget.addControl(new FigureSegmentedControl({
      label: 'destination',
      value: this.destinationName,
      options: [
        { label: 'Slate', value: 'slate' },
        { label: 'Gray', value: 'gray' },
      ],
      onChange: (value) => {
        this.destinationName = value;
        this.render();
      },
    }).element());

    this.widget.addControl(new FigureSegmentedControl({
      label: 'blend',
      value: this.blendEnabled,
      options: [
        { label: 'On', value: true },
        { label: 'Off', value: false },
      ],
      onChange: (value) => {
        this.blendEnabled = value;
        this.render();
      },
    }).element());

    this.widget.addControl(this.factorControl('source factor', this.sourceFactor, (value) => {
      this.sourceFactor = value;
      this.render();
    }));
    this.widget.addControl(this.factorControl('destination factor', this.destinationFactor, (value) => {
      this.destinationFactor = value;
      this.render();
    }));

    this.widget.addControl(new FigureSegmentedControl({
      label: 'op',
      value: this.blendOp,
      options: [
        { label: 'Add', value: 'Add' },
        { label: 'Sub', value: 'Subtract' },
        { label: 'RevSub', value: 'ReverseSubtract' },
        { label: 'Min', value: 'Min' },
        { label: 'Max', value: 'Max' },
      ],
      onChange: (value) => {
        this.blendOp = value;
        this.render();
      },
    }).element());

    this.widget.addControl(new FigureSegmentedControl({
      label: 'write',
      value: this.maskValue(),
      options: [
        { label: 'RGB', value: 'rgb' },
        { label: 'G only', value: 'g' },
        { label: 'RB', value: 'rb' },
        { label: 'None', value: 'none' },
      ],
      onChange: (value) => {
        this.writeMask = {
          r: value.includes('r'),
          g: value.includes('g'),
          b: value.includes('b'),
        };
        this.render();
      },
    }).element());

    this.widget.addControl(new FigureSliderControl({
      label: 'constant alpha',
      min: 0,
      max: 1,
      step: 0.01,
      value: this.constantAlpha,
      precision: 2,
      onChange: (value) => {
        this.constantAlpha = value;
        this.render();
      },
    }).element());

    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  factorControl(label, value, onChange) {
    const root = document.createElement('label');
    Object.assign(root.style, {
      alignItems: 'center',
      display: 'inline-flex',
      fontSize: '14px',
      gap: '6px',
    });
    const text = document.createElement('span');
    text.textContent = label;
    const select = document.createElement('select');
    select.setAttribute('data-blend-factor-control', label);
    this.factorOptions.forEach((option) => {
      const item = document.createElement('option');
      item.value = option;
      item.textContent = option;
      select.appendChild(item);
    });
    select.value = value;
    select.addEventListener('change', (event) => onChange(event.target.value));
    root.appendChild(text);
    root.appendChild(select);
    return root;
  }

  maskValue() {
    if (this.writeMask.r && this.writeMask.g && this.writeMask.b) return 'rgb';
    if (!this.writeMask.r && this.writeMask.g && !this.writeMask.b) return 'g';
    if (this.writeMask.r && !this.writeMask.g && this.writeMask.b) return 'rb';
    if (!this.writeMask.r && !this.writeMask.g && !this.writeMask.b) return 'none';
    return ['r', 'g', 'b'].filter(channel => this.writeMask[channel]).join('');
  }

  color(name) {
    return this.colors[name];
  }

  clampColor(color) {
    return {
      r: FigureMath.clamp01(color.r),
      g: FigureMath.clamp01(color.g),
      b: FigureMath.clamp01(color.b),
    };
  }

  colorCss(color) {
    const clamped = this.clampColor(color);
    const r = Math.round(clamped.r * 255);
    const g = Math.round(clamped.g * 255);
    const b = Math.round(clamped.b * 255);
    return `rgb(${r}, ${g}, ${b})`;
  }

  factor(name, channel, source, destination) {
    switch (name) {
    case 'Zero':
      return 0;
    case 'One':
      return 1;
    case 'SourceColor':
      return source[channel];
    case 'OneMinusSourceColor':
      return 1 - source[channel];
    case 'DestinationColor':
      return destination[channel];
    case 'OneMinusDestinationColor':
      return 1 - destination[channel];
    case 'ConstantColor':
      return this.constantColor[channel];
    case 'OneMinusConstantColor':
      return 1 - this.constantColor[channel];
    case 'ConstantAlpha':
      return this.constantAlpha;
    case 'OneMinusConstantAlpha':
      return 1 - this.constantAlpha;
    default:
      return 1;
    }
  }

  blend(source, destination) {
    if (!this.blendEnabled) return source;
    const result = {};
    ['r', 'g', 'b'].forEach((channel) => {
      if (this.blendOp === 'Min') {
        result[channel] = Math.min(source[channel], destination[channel]);
      } else if (this.blendOp === 'Max') {
        result[channel] = Math.max(source[channel], destination[channel]);
      } else {
        const s = source[channel] * this.factor(this.sourceFactor, channel, source, destination);
        const d = destination[channel] *
          this.factor(this.destinationFactor, channel, source, destination);
        if (this.blendOp === 'Subtract') {
          result[channel] = s - d;
        } else if (this.blendOp === 'ReverseSubtract') {
          result[channel] = d - s;
        } else {
          result[channel] = s + d;
        }
      }
    });
    return result;
  }

  applyMask(blended, destination) {
    return {
      r: this.writeMask.r ? blended.r : destination.r,
      g: this.writeMask.g ? blended.g : destination.g,
      b: this.writeMask.b ? blended.b : destination.b,
    };
  }

  outputState() {
    const source = this.color(this.sourceName);
    const destination = this.color(this.destinationName);
    const blended = this.blend(source, destination);
    const output = this.applyMask(blended, destination);
    return { source, destination, blended, output };
  }

  render() {
    this.canvas.clear();
    this.canvas.element.setAttribute('data-blend-enabled', this.blendEnabled ? '1' : '0');
    this.canvas.element.setAttribute('data-source-factor', this.sourceFactor);
    this.canvas.element.setAttribute('data-destination-factor', this.destinationFactor);
    this.canvas.element.setAttribute('data-blend-op', this.blendOp);
    this.canvas.element.setAttribute('data-write-mask', this.maskValue());

    const state = this.outputState();
    this.renderSwatch(34, 70, 'source', state.source, 'source');
    this.renderSwatch(34, 206, 'destination', state.destination, 'destination');
    this.renderEquation(218, 54, state);
    this.renderMask(468, 46, state);
    this.renderSwatch(604, 104, 'framebuffer output', state.output, 'output', 126, 126);
    this.renderChannelBars(34, 340, state);
  }

  renderSwatch(x, y, label, color, role, width = 132, height = 72) {
    this.canvas.text(x, y - 13, label, {
      'font-size': 14,
      'font-weight': 700,
    });
    this.canvas.add('rect', {
      x,
      y,
      width,
      height,
      rx: 6,
      fill: this.colorCss(color),
      stroke: '#202020',
      'stroke-width': FigurePixelStrokeWidth,
      'data-color-role': role,
    });
    this.canvas.text(x + 10, y + height + 24, this.colorText(color), {
      'font-size': 13,
      fill: '#333',
      'data-color-label': role,
    });
  }

  renderEquation(x, y, state) {
    this.canvas.panel({ x, y, width: 204, height: 196 }, 'blend equation');
    const sourceText = this.blendEnabled
      ? `source * ${this.shortFactor(this.sourceFactor)}`
      : 'source';
    const destinationText = this.blendEnabled
      ? `dest * ${this.shortFactor(this.destinationFactor)}`
      : 'destination ignored';
    const opText = this.blendEnabled ? this.blendOp : 'Replace';
    this.canvas.text(x + 16, y + 70, sourceText, {
      'font-size': 13,
      'data-equation-term': 'source',
    });
    this.canvas.text(x + 16, y + 104, opText, {
      'font-size': 18,
      'font-weight': 700,
      'data-equation-op': opText,
    });
    this.canvas.text(x + 16, y + 134, destinationText, {
      'font-size': 13,
      'data-equation-term': 'destination',
    });
    this.canvas.add('rect', {
      x: x + 16,
      y: y + 154,
      width: 54,
      height: 22,
      rx: 4,
      fill: this.colorCss(state.blended),
      stroke: '#202020',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'data-color-role': 'blended',
    });
    this.canvas.text(x + 80, y + 171, this.colorText(state.blended), {
      'font-size': 12,
      fill: '#333',
    });
  }

  renderMask(x, y, state) {
    this.canvas.panel({ x, y, width: 104, height: 218 }, 'write mask');
    const channels = [
      ['r', '#d94835'],
      ['g', '#2f9e44'],
      ['b', '#2f6edb'],
    ];
    channels.forEach(([channel, color], index) => {
      const y0 = y + 52 + index * 52;
      const enabled = this.writeMask[channel];
      this.canvas.add('rect', {
        x: x + 18,
        y: y0,
        width: 30,
        height: 30,
        rx: 4,
        fill: enabled ? color : '#e8e8e8',
        stroke: '#202020',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-write-channel': channel,
        'data-channel-enabled': enabled ? '1' : '0',
      });
      this.canvas.text(x + 58, y0 + 21, enabled ? 'write' : 'keep', {
        'font-size': 12,
        fill: '#333',
      });
    });
    this.canvas.add('rect', {
      x: x + 18,
      y: y + 180,
      width: 30,
      height: 18,
      rx: 4,
      fill: this.colorCss(this.constantColor),
      stroke: '#202020',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'data-color-role': 'constant',
    });
    this.canvas.text(x + 58, y + 194, `a=${this.constantAlpha.toFixed(2)}`, {
      'font-size': 12,
      fill: '#333',
      'data-constant-alpha': this.constantAlpha.toFixed(2),
    });
  }

  renderChannelBars(x, y, state) {
    const channels = [
      ['r', '#d94835'],
      ['g', '#2f9e44'],
      ['b', '#2f6edb'],
    ];
    this.canvas.text(x, y - 18, 'output channels', {
      'font-size': 14,
      'font-weight': 700,
    });
    channels.forEach(([channel, color], index) => {
      const x0 = x + index * 148;
      const destination = state.destination[channel];
      const blended = state.blended[channel];
      const output = state.output[channel];
      this.canvas.text(x0, y + 14, channel.toUpperCase(), {
        'font-size': 14,
        'font-weight': 700,
        fill: color,
      });
      this.bar(x0 + 26, y, 96, destination, '#d7dbe0', 'destination');
      this.bar(x0 + 26, y + 18, 96, blended, '#a8d0ff', 'blended');
      this.bar(x0 + 26, y + 36, 96, output, color, 'output');
    });
  }

  bar(x, y, width, value, fill, role) {
    this.canvas.add('rect', {
      x,
      y,
      width,
      height: 10,
      fill: '#ececec',
      stroke: '#c8c8c8',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.canvas.add('rect', {
      x,
      y,
      width: FigureMath.clamp01(value) * width,
      height: 10,
      fill,
      stroke: 'none',
      'data-channel-bar': role,
    });
  }

  shortFactor(factor) {
    return factor
      .replace('OneMinus', '1-')
      .replace('Source', 'src')
      .replace('Destination', 'dst')
      .replace('Constant', 'const');
  }

  colorText(color) {
    return `rgb ${this.clampColor(color).r.toFixed(2)} ` +
      `${this.clampColor(color).g.toFixed(2)} ` +
      `${this.clampColor(color).b.toFixed(2)}`;
  }
}

((scriptElement) => {
  const figure = new RasterizerColorOutputWidget();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
