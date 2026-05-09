// Interactive widget for rasterizer directional-light shadow maps.
//
// The production renderer builds a depth-only image from each directional
// light, then the camera pass projects every shaded point into that image. This
// widget keeps the geometry 2D so the important comparison is visible:
// receiver depth <= stored depth + bias means lit; otherwise the receiver is in
// shadow.

class RasterizerShadowMap {
  constructor() {
    this.width = 760;
    this.height = 360;
    this.world = { left: 36, top: 46, right: 476, bottom: 292 };
    this.groundY = 260;
    this.mapPanel = { x: 520, y: 54, width: 196, height: 112 };
    this.comparePanel = { x: 520, y: 188, width: 196, height: 104 };
    this.receiverX = 382;
    this.caster = { x: 236, width: 58, height: 92 };
    this.mapSize = 16;
    this.bias = 10;
    this.lightDirectionToLight = new Vector(-0.56, -0.83).normalized();
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'rasterizer-shadow-map-widget' });
    this.sizeControl = new FigureSegmentedControl({
      label: 'shadow map size',
      value: this.mapSize,
      options: [8, 16, 32].map(size => ({ label: `${size} texels`, value: size })),
      onChange: (value) => {
        this.mapSize = Number(value);
        this.render();
      },
    });
    this.biasControl = new FigureSliderControl({
      label: 'depth bias',
      min: 0,
      max: 30,
      step: 1,
      value: this.bias,
      precision: 0,
      onChange: (value) => {
        this.bias = value;
        this.render();
      },
    });

    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });
    this.widget.addControl(this.sizeControl.element());
    this.widget.addControl(this.biasControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  lightForward() {
    return this.lightDirectionToLight.multiply(-1).normalized();
  }

  lightRight() {
    const forward = this.lightForward();
    return new Vector(forward.y, -forward.x).normalized();
  }

  casterRect() {
    return {
      left: this.caster.x - this.caster.width / 2,
      right: this.caster.x + this.caster.width / 2,
      top: this.groundY - this.caster.height,
      bottom: this.groundY,
    };
  }

  lightSpace(point) {
    const p = Vector.from(point);
    return {
      s: p.dot(this.lightRight()),
      depth: p.dot(this.lightForward()),
    };
  }

  sRange() {
    const corners = [
      new Vector(this.world.left, this.world.top),
      new Vector(this.world.right, this.world.top),
      new Vector(this.world.left, this.world.bottom),
      new Vector(this.world.right, this.world.bottom),
    ].map(point => this.lightSpace(point).s);
    return {
      min: Math.min(...corners),
      max: Math.max(...corners),
    };
  }

  binForS(s) {
    const range = this.sRange();
    const t = (s - range.min) / (range.max - range.min);
    return Math.floor(FigureMath.clamp(t, 0, 0.999999) * this.mapSize);
  }

  floorPointForS(s) {
    const right = this.lightRight();
    const x = (s - this.groundY * right.y) / right.x;
    if (x < this.world.left || x > this.world.right) return null;
    return new Vector(x, this.groundY);
  }

  sampleCasterPoints() {
    const rect = this.casterRect();
    const points = [];
    const steps = 42;
    for (let i = 0; i <= steps; ++i) {
      const t = i / steps;
      const x = FigureMath.lerp(rect.left, rect.right, t);
      const y = FigureMath.lerp(rect.top, rect.bottom, t);
      points.push(new Vector(x, rect.top));
      points.push(new Vector(rect.left, y));
      points.push(new Vector(rect.right, y));
    }
    return points;
  }

  buildDepthMap() {
    const range = this.sRange();
    const bins = Array.from({ length: this.mapSize }, (_, index) => {
      const s0 = FigureMath.lerp(range.min, range.max, index / this.mapSize);
      const s1 = FigureMath.lerp(range.min, range.max, (index + 1) / this.mapSize);
      const center = (s0 + s1) / 2;
      const floorPoint = this.floorPointForS(center);
      return {
        index,
        s0,
        s1,
        floorDepth: floorPoint ? this.lightSpace(floorPoint).depth : Infinity,
        casterDepth: Infinity,
        storedDepth: Infinity,
        owner: 'empty',
      };
    });

    for (const point of this.sampleCasterPoints()) {
      const lightPoint = this.lightSpace(point);
      const index = this.binForS(lightPoint.s);
      bins[index].casterDepth = Math.min(bins[index].casterDepth, lightPoint.depth);
    }

    bins.forEach((bin) => {
      if (bin.casterDepth < bin.floorDepth) {
        bin.storedDepth = bin.casterDepth;
        bin.owner = 'caster';
      } else if (Number.isFinite(bin.floorDepth)) {
        bin.storedDepth = bin.floorDepth;
        bin.owner = 'floor';
      }
    });

    return bins;
  }

  receiverPoint() {
    return new Vector(this.receiverX, this.groundY);
  }

  receiverState(depthMap = this.buildDepthMap()) {
    const receiver = this.receiverPoint();
    const lightPoint = this.lightSpace(receiver);
    const bin = depthMap[this.binForS(lightPoint.s)];
    const threshold = bin.storedDepth + this.bias;
    return {
      receiver,
      lightPoint,
      bin,
      lit: !Number.isFinite(bin.storedDepth) || lightPoint.depth <= threshold,
      threshold,
    };
  }

  render() {
    this.canvas.clear();
    this.canvas.element.setAttribute('data-shadow-map-size', this.mapSize);
    this.canvas.element.setAttribute('data-shadow-bias', this.bias);

    const depthMap = this.buildDepthMap();
    const state = this.receiverState(depthMap);
    this.canvas.element.setAttribute('data-shadow-result', state.lit ? 'lit' : 'shadowed');

    this.renderWorld(depthMap, state);
    this.renderDepthMap(depthMap, state);
    this.renderComparison(state);
    this.renderHandles();
  }

  renderWorld(depthMap, state) {
    this.canvas.panel({
      x: this.world.left,
      y: this.world.top,
      width: this.world.right - this.world.left,
      height: this.world.bottom - this.world.top,
    }, 'camera pass');

    this.renderShadowedGroundBins(depthMap);
    this.canvas.line(new Vector(this.world.left + 18, this.groundY),
                     new Vector(this.world.right - 18, this.groundY), {
      stroke: '#333',
    });

    this.renderLightRay(new Vector(94, 96), new Vector(148, 176), 'light rays');
    this.renderCaster();
    this.renderReceiver(state);

    this.canvas.text(this.world.left + 16, this.groundY + 28,
                     'receiver point projects into the light depth map', {
      'font-size': 15,
      fill: '#444',
    });
  }

  renderShadowedGroundBins(depthMap) {
    for (const bin of depthMap) {
      if (bin.owner !== 'caster') continue;

      const p0 = this.floorPointForS(bin.s0);
      const p1 = this.floorPointForS(bin.s1);
      if (!p0 || !p1) continue;

      const left = Math.max(this.world.left + 18, Math.min(p0.x, p1.x));
      const right = Math.min(this.world.right - 18, Math.max(p0.x, p1.x));
      if (right <= left) continue;

      this.canvas.add('rect', {
        x: left,
        y: this.groundY - 9,
        width: right - left,
        height: 18,
        fill: '#1c7ed6',
        'fill-opacity': 0.26,
        'data-shadowed-ground-bin': bin.index,
      });
    }
  }

  renderLightRay(from, to, label) {
    this.canvas.arrow(from, to, {
      stroke: '#f08c00',
      'stroke-width': FigurePixelStrokeWidth,
      markerId: 'rasterizer-shadow-light-arrow',
      markerColor: '#f08c00',
    });
    this.canvas.text(from.x - 8, from.y - 12, label, {
      'font-size': 14,
      fill: '#9c5b00',
    });
  }

  renderCaster() {
    const rect = this.casterRect();
    this.canvas.add('rect', {
      x: rect.left,
      y: rect.top,
      width: rect.right - rect.left,
      height: rect.bottom - rect.top,
      fill: '#e7f0ff',
      stroke: '#1c5fd1',
      'stroke-width': FigurePixelStrokeWidth,
      'data-shadow-caster': '1',
    });
    this.canvas.text(rect.left + 8, rect.top + 22, 'caster', {
      'font-size': 15,
      'font-weight': 700,
      fill: '#1c5fd1',
    });
  }

  renderReceiver(state) {
    const color = state.lit ? '#2f9e44' : '#e03131';
    const rayEnd = state.receiver.plus(this.lightDirectionToLight.multiply(150));
    this.canvas.line(state.receiver, rayEnd, {
      stroke: color,
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '6 5',
      'data-receiver-light-ray': state.lit ? 'lit' : 'shadowed',
    });
    this.canvas.text(state.receiver.x - 42, state.receiver.y - 22, 'receiver', {
      'font-size': 15,
      fill: color,
    });
  }

  renderDepthMap(depthMap, state) {
    this.canvas.panel(this.mapPanel, 'light pass');
    this.canvas.text(this.mapPanel.x + 16, this.mapPanel.y + 50, 'one nearest depth per texel', {
      'font-size': 14,
      fill: '#444',
    });

    const strip = {
      x: this.mapPanel.x + 16,
      y: this.mapPanel.y + 66,
      width: this.mapPanel.width - 32,
      height: 38,
    };
    const minDepth = Math.min(...depthMap
      .map(bin => bin.storedDepth)
      .filter(depth => Number.isFinite(depth)));
    const maxDepth = Math.max(...depthMap
      .map(bin => bin.storedDepth)
      .filter(depth => Number.isFinite(depth)));

    depthMap.forEach((bin) => {
      const x = strip.x + bin.index * strip.width / this.mapSize;
      const w = strip.width / this.mapSize;
      const t = Number.isFinite(bin.storedDepth) && maxDepth > minDepth
        ? (bin.storedDepth - minDepth) / (maxDepth - minDepth)
        : 1;
      const shade = Math.round(FigureMath.lerp(78, 224, t));
      this.canvas.add('rect', {
        x,
        y: strip.y,
        width: w,
        height: strip.height,
        fill: bin.owner === 'caster' ? '#74c0fc' : `rgb(${shade}, ${shade}, ${shade})`,
        stroke: '#ffffff',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-shadow-map-texel': bin.index,
        'data-shadow-map-owner': bin.owner,
      });
    });

    const receiverX = strip.x + state.bin.index * strip.width / this.mapSize;
    this.canvas.add('rect', {
      x: receiverX,
      y: strip.y - 4,
      width: strip.width / this.mapSize,
      height: strip.height + 8,
      fill: 'none',
      stroke: '#e03131',
      'stroke-width': FigurePixelStrokeWidth,
      'data-receiver-texel': state.bin.index,
    });
  }

  renderComparison(state) {
    this.canvas.panel(this.comparePanel, 'depth test');

    const receiverDepth = Math.round(state.lightPoint.depth);
    const threshold = Number.isFinite(state.threshold)
      ? Math.round(state.threshold)
      : 'none';
    const result = state.lit ? 'lit' : 'shadowed';
    const color = state.lit ? '#2f9e44' : '#e03131';

    this.canvas.text(this.comparePanel.x + 16, this.comparePanel.y + 52,
                     `receiver depth ${receiverDepth}`, {
      'font-size': 14,
      fill: '#333',
    });
    this.canvas.text(this.comparePanel.x + 16, this.comparePanel.y + 76,
                     `stored + bias ${threshold}`, {
      'font-size': 14,
      fill: '#333',
    });
    this.canvas.text(this.comparePanel.x + 16, this.comparePanel.y + 100,
                     result, {
      'font-size': 17,
      'font-weight': 700,
      fill: color,
      'data-shadow-readout': result,
    });
  }

  renderHandles() {
    const receiverHandle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: this.receiverPoint(),
      radius: 8,
      attrs: {
        fill: '#fff3bf',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'shadow-receiver',
      },
      onDrag: (point) => {
        this.receiverX = FigureMath.clamp(point.x, this.world.left + 28, this.world.right - 28);
        this.render();
      },
    });
    this.canvas.append(receiverHandle.element());

    const rect = this.casterRect();
    const casterHandlePoint = new Vector(this.caster.x, rect.top);
    const casterHandle = new FigureDraggablePoint({
      canvas: this.canvas,
      point: casterHandlePoint,
      radius: 8,
      attrs: {
        fill: '#d0ebff',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'shadow-caster',
      },
      onDrag: (point) => {
        this.caster.x = FigureMath.clamp(point.x, this.world.left + 84, this.world.right - 84);
        this.caster.height = FigureMath.clamp(this.groundY - point.y, 44, 138);
        this.render();
      },
    });
    this.canvas.append(casterHandle.element());
  }
}

((scriptElement) => {
  const widget = new RasterizerShadowMap();
  scriptElement.parentNode.appendChild(widget.element());
})(document.currentScript);
