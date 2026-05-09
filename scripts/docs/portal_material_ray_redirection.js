// Interactive widget for PortalMaterial ray redirection.
//
// A portal material does not sample an image at a screen-space border. When a
// ray hits the portal surface, the material starts a new scene query from the
// hit point after applying the inverse portal transform to both the ray origin
// and its direction. The returned color is optionally multiplied by the filter.

class PortalMaterialRayRedirection {
  constructor() {
    this.width = 680;
    this.height = 320;
    this.sceneOrigin = { x: 36, y: 36 };
    this.sceneSize = { width: 286, height: 248 };
    this.queryOrigin = { x: 380, y: 36 };
    this.querySize = { width: 264, height: 248 };
    this.portalCenter = { x: 180, y: 160 };
    this.portalAngle = -0.25;
    this.portalHalfLength = 72;
    this.raySource = { x: 72, y: 238 };
    this.hitOffset = 18;
    this.filter = 'warm';
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'portal-material-ray-redirection-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.filterControl = new FigureSegmentedControl({
      label: 'filter',
      value: this.filter,
      options: [
        { label: 'White', value: 'white' },
        { label: 'Warm', value: 'warm' },
        { label: 'Blue', value: 'blue' },
      ],
      onChange: (value) => {
        this.filter = value;
        this.render();
      },
    });

    this.widget.addControl(this.filterControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  portalTangent() {
    return new Vector(Math.cos(this.portalAngle), Math.sin(this.portalAngle));
  }

  portalNormal() {
    return this.portalTangent().perp();
  }

  hitPoint() {
    return Vector.from(this.portalCenter).plus(this.portalTangent().multiply(this.hitOffset));
  }

  transformedRay() {
    const hit = this.hitPoint();
    const sourceDirection = hit.minus(this.raySource).safeNormalized(Vector.right);
    const localOrigin = hit.minus(this.portalCenter).rotated(-this.portalAngle);
    const localDirection = sourceDirection.rotated(-this.portalAngle);
    return {
      origin: {
        x: this.queryOrigin.x + this.querySize.width * 0.42 + localOrigin.x * 0.75,
        y: this.queryOrigin.y + this.querySize.height * 0.56 + localOrigin.y * 0.75,
      },
      direction: localDirection.safeNormalized(Vector.right),
    };
  }

  filterColor() {
    if (this.filter === 'blue') return '#9ec5ff';
    if (this.filter === 'warm') return '#ffe1a6';
    return '#ffffff';
  }

  addText(x, y, text, attrs = {}) {
    const element = this.canvas.add('text', {
      x,
      y,
      'font-family': 'sans-serif',
      'font-size': 14,
      fill: '#222',
      ...attrs,
    });
    element.textContent = text;
    return element;
  }

  addArrowMarker() {
    const defs = this.canvas.add('defs');
    const marker = createSvgElement('marker', {
      id: 'portal-ray-arrow',
      markerWidth: 10,
      markerHeight: 10,
      refX: 8,
      refY: 3,
      orient: 'auto',
      markerUnits: 'strokeWidth',
    });
    const path = createSvgElement('path', {
      d: 'M0,0 L0,6 L9,3 z',
      fill: '#111',
    });
    marker.appendChild(path);
    defs.appendChild(marker);
  }

  panel(origin, size, title) {
    this.canvas.add('rect', {
      x: origin.x,
      y: origin.y,
      width: size.width,
      height: size.height,
      rx: 4,
      fill: '#f8fbff',
      stroke: '#222',
      'stroke-width': FigurePixelStrokeWidth,
    });
    this.addText(origin.x, origin.y - 12, title);
  }

  render() {
    this.canvas.clear();
    this.addArrowMarker();
    this.panel(this.sceneOrigin, this.sceneSize, 'Scene ray hits portal material');
    this.panel(this.queryOrigin, this.querySize, 'Transformed scene query');
    this.renderPortalPanel();
    this.renderQueryPanel();
    this.renderFilter();
  }

  renderPortalPanel() {
    const tangent = this.portalTangent();
    const normal = this.portalNormal();
    const portalStart = Vector.from(this.portalCenter).plus(tangent.multiply(-this.portalHalfLength));
    const portalEnd = Vector.from(this.portalCenter).plus(tangent.multiply(this.portalHalfLength));
    const hit = this.hitPoint();

    this.canvas.add('line', {
      x1: portalStart.x,
      y1: portalStart.y,
      x2: portalEnd.x,
      y2: portalEnd.y,
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth + 2,
      'data-portal-plane': 'true',
    });
    this.canvas.add('line', {
      x1: portalStart.x,
      y1: portalStart.y,
      x2: portalEnd.x,
      y2: portalEnd.y,
      stroke: this.filterColor(),
      'stroke-width': FigurePixelStrokeWidth,
      'data-filter-stroke': this.filter,
    });
    this.addText(portalStart.x - 16, portalStart.y - 14, 'portal plane');

    this.canvas.add('line', {
      x1: this.raySource.x,
      y1: this.raySource.y,
      x2: hit.x,
      y2: hit.y,
      stroke: '#d22',
      'stroke-width': FigurePixelStrokeWidth,
      'marker-end': 'url(#portal-ray-arrow)',
      'data-incoming-ray': 'true',
    });
    this.canvas.add('line', {
      x1: hit.x,
      y1: hit.y,
      x2: hit.x + normal.x * 48,
      y2: hit.y + normal.y * 48,
      stroke: '#777',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '5 5',
    });
    this.addText(this.raySource.x - 12, this.raySource.y + 22, 'ray');
    this.addText(hit.x + 10, hit.y - 10, 'hit point');

    this.renderHandle(this.raySource, 'source-ray-origin', {
      fill: '#d22',
      onDrag: (point) => {
        this.raySource = {
          x: FigureMath.clamp(point.x, this.sceneOrigin.x + 18, this.sceneOrigin.x + this.sceneSize.width - 18),
          y: FigureMath.clamp(point.y, this.sceneOrigin.y + 18, this.sceneOrigin.y + this.sceneSize.height - 18),
        };
        this.render();
      },
    });
    this.renderHandle(hit, 'source-ray-hit', {
      fill: '#f08c00',
      onDrag: (point) => {
        const offset = Vector.from(point).minus(this.portalCenter).dot(tangent);
        this.hitOffset = FigureMath.clamp(offset, -this.portalHalfLength + 10, this.portalHalfLength - 10);
        this.render();
      },
    });
    this.renderHandle(this.portalCenter, 'portal-transform-origin', {
      fill: '#fff',
      onDrag: (point) => {
        this.portalCenter = {
          x: FigureMath.clamp(point.x, this.sceneOrigin.x + 90, this.sceneOrigin.x + this.sceneSize.width - 90),
          y: FigureMath.clamp(point.y, this.sceneOrigin.y + 70, this.sceneOrigin.y + this.sceneSize.height - 70),
        };
        this.render();
      },
    });

    const rotationHandle = Vector.from(this.portalCenter).plus(tangent.multiply(this.portalHalfLength + 26));
    this.canvas.add('line', {
      x1: this.portalCenter.x,
      y1: this.portalCenter.y,
      x2: rotationHandle.x,
      y2: rotationHandle.y,
      stroke: '#2060d0',
      'stroke-width': FigurePixelGuideStrokeWidth,
      'stroke-dasharray': '5 5',
    });
    this.renderHandle(rotationHandle, 'portal-transform-rotation', {
      fill: '#2060d0',
      onDrag: (point) => {
        const delta = Vector.from(point).minus(this.portalCenter);
        this.portalAngle = Math.atan2(delta.y, delta.x);
        this.render();
      },
    });
  }

  renderQueryPanel() {
    const ray = this.transformedRay();
    const end = Vector.from(ray.origin).plus(ray.direction.multiply(138));

    this.canvas.add('line', {
      x1: this.queryOrigin.x + 28,
      y1: this.queryOrigin.y + this.querySize.height * 0.56,
      x2: this.queryOrigin.x + this.querySize.width - 28,
      y2: this.queryOrigin.y + this.querySize.height * 0.56,
      stroke: '#d7dee8',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.canvas.add('line', {
      x1: this.queryOrigin.x + this.querySize.width * 0.42,
      y1: this.queryOrigin.y + 24,
      x2: this.queryOrigin.x + this.querySize.width * 0.42,
      y2: this.queryOrigin.y + this.querySize.height - 24,
      stroke: '#d7dee8',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.canvas.add('circle', {
      cx: ray.origin.x,
      cy: ray.origin.y,
      r: 7,
      fill: '#f08c00',
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      'data-transformed-origin': 'true',
    });
    this.canvas.add('line', {
      x1: ray.origin.x,
      y1: ray.origin.y,
      x2: end.x,
      y2: end.y,
      stroke: '#2060d0',
      'stroke-width': FigurePixelStrokeWidth,
      'marker-end': 'url(#portal-ray-arrow)',
      'data-transformed-direction': 'true',
    });
    this.addText(ray.origin.x + 10, ray.origin.y - 10, 'transformed origin');
    this.addText(end.x - 72, end.y - 12, 'transformed direction');
    this.addText(this.queryOrigin.x + 22, this.queryOrigin.y + this.querySize.height - 20,
      'Scene is asked what this new ray sees', { 'font-family': 'monospace' });
  }

  renderFilter() {
    const x = 36;
    const y = 296;
    this.addText(x, y, 'returned color x filter');
    this.canvas.add('rect', {
      x: x + 150,
      y: y - 18,
      width: 44,
      height: 20,
      fill: this.filterColor(),
      stroke: '#111',
      'stroke-width': FigurePixelStrokeWidth,
      'data-filter-swatch': this.filter,
    });
  }

  renderHandle(point, name, { fill, onDrag }) {
    const handle = new FigureDraggablePoint({
      canvas: this.canvas,
      point,
      radius: 9,
      attrs: {
        fill,
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        'data-drag-handle': name,
      },
      onDrag,
    });
    this.canvas.append(handle.element());
  }
}

((scriptElement) => {
  const figure = new PortalMaterialRayRedirection();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
