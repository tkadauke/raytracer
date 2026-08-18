// Interactive widget for the wide-angle camera projection docs. It compares
// the image-plane coordinate a camera receives with the unit-sphere direction
// that the camera traces.

class WideAngleCameraMappings {
  constructor() {
    this.mode = 'fisheye';
    this.fishEyeFov = 180;
    this.sphericalHorizontalFov = 180;
    this.sphericalVerticalFov = 120;
    this.point = { x: 0.68, y: 0.34 };

    this.image = { x: 28, y: 34, width: 300, height: 190 };
    this.sphere = { cx: 500, cy: 129, r: 88 };
    this.width = 620;
    this.height = 270;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'wide-angle-camera-mappings-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.modeControl = new FigureSegmentedControl({
      label: 'camera',
      value: this.mode,
      options: [
        { label: 'fisheye', value: 'fisheye' },
        { label: 'spherical', value: 'spherical' },
        { label: 'equirectangular', value: 'equirectangular' },
      ],
      onChange: (value) => {
        this.mode = value;
        this.syncControls();
        this.render();
      },
    });

    this.fishEyeSlider = new FigureSliderControl({
      label: 'fieldOfView',
      min: 30,
      max: 330,
      step: 1,
      value: this.fishEyeFov,
      precision: 0,
      format: value => `${value.toFixed(0)}°`,
      onChange: (value) => {
        this.fishEyeFov = value;
        this.render();
      },
    });

    this.sphericalHorizontalSlider = new FigureSliderControl({
      label: 'horizontalFieldOfView',
      min: 30,
      max: 360,
      step: 1,
      value: this.sphericalHorizontalFov,
      precision: 0,
      format: value => `${value.toFixed(0)}°`,
      onChange: (value) => {
        this.sphericalHorizontalFov = value;
        this.render();
      },
    });

    this.sphericalVerticalSlider = new FigureSliderControl({
      label: 'verticalFieldOfView',
      min: 30,
      max: 180,
      step: 1,
      value: this.sphericalVerticalFov,
      precision: 0,
      format: value => `${value.toFixed(0)}°`,
      onChange: (value) => {
        this.sphericalVerticalFov = value;
        this.render();
      },
    });

    this.widget.addControl(this.modeControl.element());
    this.widget.addControl(this.fishEyeSlider.element());
    this.widget.addControl(this.sphericalHorizontalSlider.element());
    this.widget.addControl(this.sphericalVerticalSlider.element());
    this.widget.setContent(this.canvas.element);
    this.syncControls();
    this.render();
    return this.widget.root;
  }

  syncControls() {
    if (!this.fishEyeSlider.root) return;
    this.fishEyeSlider.root.style.display = this.mode === 'fisheye' ? 'flex' : 'none';
    const sphericalDisplay = this.mode === 'spherical' ? 'flex' : 'none';
    this.sphericalHorizontalSlider.root.style.display = sphericalDisplay;
    this.sphericalVerticalSlider.root.style.display = sphericalDisplay;
  }

  radians(degrees) {
    return degrees * Math.PI / 180.0;
  }

  imagePoint() {
    return {
      x: this.image.x + this.point.x * this.image.width,
      y: this.image.y + this.point.y * this.image.height,
    };
  }

  setImagePoint(svgPoint) {
    this.point = {
      x: FigureMath.clamp((svgPoint.x - this.image.x) / this.image.width, 0, 1),
      y: FigureMath.clamp((svgPoint.y - this.image.y) / this.image.height, 0, 1),
    };
  }

  normalizedImagePoint() {
    return {
      x: this.point.x * 2.0 - 1.0,
      y: this.point.y * 2.0 - 1.0,
    };
  }

  direction() {
    if (this.mode === 'fisheye') return this.fishEyeDirection();
    if (this.mode === 'spherical') return this.sphericalDirection();
    return this.equirectangularDirection();
  }

  fishEyeDirection() {
    const p = this.normalizedImagePoint();
    const r = Math.sqrt(p.x * p.x + p.y * p.y);
    if (r > 1.0) return null;
    if (r < 1e-9) return { x: 0, y: 0, z: 1 };

    const psi = r * this.radians(this.fishEyeFov) / 2.0;
    const sinPsi = Math.sin(psi);
    return {
      x: sinPsi * p.x / r,
      y: sinPsi * p.y / r,
      z: Math.cos(psi),
    };
  }

  sphericalDirection() {
    const p = this.normalizedImagePoint();
    const lon = p.x * this.radians(this.sphericalHorizontalFov) / 2.0;
    const lat = -p.y * this.radians(this.sphericalVerticalFov) / 2.0;
    const cosLat = Math.cos(lat);
    return {
      x: Math.sin(lon) * cosLat,
      y: -Math.sin(lat),
      z: Math.cos(lon) * cosLat,
    };
  }

  equirectangularDirection() {
    const lon = this.normalizedImagePoint().x * Math.PI;
    const lat = (1.0 - 2.0 * this.point.y) * Math.PI / 2.0;
    const cosLat = Math.cos(lat);
    return {
      x: Math.sin(lon) * cosLat,
      y: -Math.sin(lat),
      z: Math.cos(lon) * cosLat,
    };
  }

  spherePoint(direction) {
    return {
      x: this.sphere.cx + direction.x * this.sphere.r,
      y: this.sphere.cy + direction.y * this.sphere.r,
    };
  }

  render() {
    this.canvas.clear();
    this.renderImagePlane();
    this.renderSphere();
    this.renderMapping();
  }

  renderImagePlane() {
    this.canvas.add('text', {
      x: this.image.x,
      y: 20,
      'font-family': 'sans-serif',
      'font-size': 14,
      fill: '#222',
      textContent: 'image rectangle',
    });

    this.canvas.add('rect', {
      x: this.image.x,
      y: this.image.y,
      width: this.image.width,
      height: this.image.height,
      fill: '#fff',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
    });

    this.renderImageGuides();

    const point = this.imagePoint();
    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point,
      radius: 8,
      attrs: {
        fill: '#f59f00',
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': 'image-point',
      },
      onDrag: (svgPoint) => {
        this.setImagePoint(svgPoint);
        this.render();
      },
    });
    this.canvas.append(handle.element());
  }

  renderImageGuides() {
    if (this.mode === 'fisheye') {
      const cx = this.image.x + this.image.width / 2.0;
      const cy = this.image.y + this.image.height / 2.0;
      const r = Math.min(this.image.width, this.image.height) / 2.0;
      this.canvas.add('circle', {
        cx,
        cy,
        r,
        fill: '#d8f5a2',
        'fill-opacity': 0.3,
        stroke: '#2f9e44',
        'stroke-width': FigurePixelStrokeWidth,
        'data-fisheye-valid-disc': '1',
      });
      this.canvas.add('text', {
        x: this.image.x + 12,
        y: this.image.y + this.image.height + 25,
        'font-family': 'sans-serif',
        'font-size': 13,
        fill: '#444',
        textContent: 'outside the disc: no primary ray',
      });
    } else if (this.mode === 'spherical') {
      this.canvas.add('rect', {
        x: this.image.x,
        y: this.image.y,
        width: this.image.width,
        height: this.image.height,
        fill: '#e7f5ff',
        'fill-opacity': 0.55,
        stroke: '#1c7ed6',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-partial-panorama': '1',
      });
      this.canvas.add('text', {
        x: this.image.x + 12,
        y: this.image.y + this.image.height + 25,
        'font-family': 'sans-serif',
        'font-size': 13,
        fill: '#444',
        textContent: 'FOV selects a partial panorama window',
      });
    } else {
      this.canvas.add('rect', {
        x: this.image.x,
        y: this.image.y,
        width: this.image.width,
        height: 18,
        fill: '#ffe3e3',
        stroke: 'none',
        'data-pole-stretch': 'north',
      });
      this.canvas.add('rect', {
        x: this.image.x,
        y: this.image.y + this.image.height - 18,
        width: this.image.width,
        height: 18,
        fill: '#ffe3e3',
        stroke: 'none',
        'data-pole-stretch': 'south',
      });
      [this.image.x, this.image.x + this.image.width].forEach((x) => {
        this.canvas.add('line', {
          x1: x,
          y1: this.image.y,
          x2: x,
          y2: this.image.y + this.image.height,
          stroke: '#e03131',
          'stroke-width': FigurePixelStrokeWidth,
          'stroke-dasharray': '6 5',
          'data-equirectangular-seam': '1',
        });
      });
      this.canvas.add('text', {
        x: this.image.x + 12,
        y: this.image.y + this.image.height + 25,
        'font-family': 'sans-serif',
        'font-size': 13,
        fill: '#444',
        textContent: 'left/right edges meet; top/bottom rows collapse to poles',
      });
    }
  }

  renderSphere() {
    this.canvas.add('text', {
      x: this.sphere.cx - this.sphere.r,
      y: 20,
      'font-family': 'sans-serif',
      'font-size': 14,
      fill: '#222',
      textContent: 'unit sphere direction',
    });

    this.canvas.add('circle', {
      cx: this.sphere.cx,
      cy: this.sphere.cy,
      r: this.sphere.r,
      fill: '#fff',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
    });

    for (const scale of [-0.5, 0, 0.5]) {
      this.canvas.add('ellipse', {
        cx: this.sphere.cx,
        cy: this.sphere.cy,
        rx: this.sphere.r * Math.sqrt(1 - scale * scale),
        ry: this.sphere.r * 0.18,
        fill: 'none',
        stroke: '#adb5bd',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
      this.canvas.add('ellipse', {
        cx: this.sphere.cx,
        cy: this.sphere.cy,
        rx: this.sphere.r * 0.18,
        ry: this.sphere.r * Math.sqrt(1 - scale * scale),
        fill: 'none',
        stroke: '#adb5bd',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
    }

    this.canvas.add('text', {
      x: this.sphere.cx - 8,
      y: this.sphere.cy - this.sphere.r - 8,
      'font-family': 'sans-serif',
      'font-size': 12,
      fill: '#555',
      textContent: 'up',
    });
    this.canvas.add('text', {
      x: this.sphere.cx - 18,
      y: this.sphere.cy + this.sphere.r + 20,
      'font-family': 'sans-serif',
      'font-size': 12,
      fill: '#555',
      textContent: 'down',
    });
  }

  renderMapping() {
    const direction = this.direction();
    const imagePoint = this.imagePoint();
    if (!direction) {
      this.canvas.add('line', {
        x1: imagePoint.x - 10,
        y1: imagePoint.y - 10,
        x2: imagePoint.x + 10,
        y2: imagePoint.y + 10,
        stroke: '#e03131',
        'stroke-width': FigurePixelStrokeWidth,
      });
      this.canvas.add('line', {
        x1: imagePoint.x + 10,
        y1: imagePoint.y - 10,
        x2: imagePoint.x - 10,
        y2: imagePoint.y + 10,
        stroke: '#e03131',
        'stroke-width': FigurePixelStrokeWidth,
      });
      this.canvas.add('text', {
        x: this.sphere.cx - this.sphere.r,
        y: this.sphere.cy + this.sphere.r + 42,
        'font-family': 'sans-serif',
        'font-size': 13,
        fill: '#e03131',
        textContent: 'cut off: fisheye pixels outside the unit disc are undefined',
      });
      return;
    }

    const spherePoint = this.spherePoint(direction);
    const markerFill = direction.z >= 0 ? '#f03e3e' : '#ffffff';
    const markerDash = direction.z >= 0 ? undefined : '5 4';
    this.canvas.add('line', {
      x1: this.sphere.cx,
      y1: this.sphere.cy,
      x2: spherePoint.x,
      y2: spherePoint.y,
      stroke: '#f03e3e',
      'stroke-width': FigurePixelStrokeWidth,
      'stroke-dasharray': markerDash,
      'data-ray-direction': '1',
    });
    this.canvas.add('circle', {
      cx: spherePoint.x,
      cy: spherePoint.y,
      r: 7,
      fill: markerFill,
      stroke: '#f03e3e',
      'stroke-width': FigurePixelStrokeWidth,
      'data-ray-direction-marker': '1',
      'data-hemisphere': direction.z >= 0 ? 'front' : 'back',
    });

    const label = this.directionLabel(direction);
    this.canvas.add('text', {
      x: this.sphere.cx - this.sphere.r,
      y: this.sphere.cy + this.sphere.r + 42,
      'font-family': 'monospace',
      'font-size': 13,
      fill: '#333',
      textContent: label,
    });
  }

  directionLabel(direction) {
    const signed = value => `${value >= 0 ? '+' : ''}${value.toFixed(2)}`;
    return `ray = (${signed(direction.x)}, ${signed(direction.y)}, ${signed(direction.z)})`;
  }
}

((scriptElement) => {
  const figure = new WideAngleCameraMappings();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
