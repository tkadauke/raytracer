// Interactive-widget primitives shared by every `scripts/docs/*.js`
// widget embedded in the Doxygen output. See `scripts/README.md` for
// the writing-a-widget recipe and `scripts/test/test_figure_js.js`
// for the test cases. Every primitive uses native ES6 class syntax
// (the historical `Class()` factory was removed in commit %h after
// all 20 widgets migrated to native syntax).

'use strict';

const FigureStrokeWidth = 0.05;
const FigureGuideStrokeWidth = 0.035;
const FigurePixelStrokeWidth = 2;
const FigurePixelGuideStrokeWidth = 1;

// CSS injection — runs at script-load time. Wrapped so it no-ops in
// non-browser environments (Node test runner): the test shim
// provides a `document` but doesn't simulate `<style>` parsing.
if (typeof document !== 'undefined' && document.head) {
  const style = document.createElement('style');
  style.type = 'text/css';
  style.innerHTML = `
svg.figure-canvas * {
  stroke-width: ${FigureStrokeWidth};
}

svg.figure-canvas .dashed {
  stroke-dasharray: 0.1, 0.1;
}

svg.figure-canvas .red {
  stroke: #ff0000;
}

svg.figure-canvas .red marker {
  stroke: #ff0000;
}

svg.figure-canvas .blue {
  stroke: #2060d0;
}

svg.figure-canvas .blue marker {
  stroke: #2060d0;
}

svg.figure-canvas .green {
  stroke: #20a050;
}

svg.figure-canvas .green marker {
  stroke: #20a050;
}

svg.figure-canvas text {
  font-size: 3.3%;
}

svg.figure-canvas line {
  stroke: #000000;
}

svg.figure-canvas line.arrow {
  marker-end: url(#arrow);
}

svg.figure-canvas line.axis {
  stroke-width: ${FigureStrokeWidth};
  marker-end: url(#arrow);
}

svg.figure-canvas circle {
  stroke: #000000;
  fill: transparent;
}

svg.figure-canvas circle.intersection {
  stroke: #000000;
  fill: #000000;
}

svg.figure-canvas circle.result {
  stroke: #ff0000;
  fill: #ff0000;
}

svg.figure-canvas rect {
  stroke: #000000;
  fill: transparent;
}

.figure-widget {
  color: #202020;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  max-width: 100%;
}

.figure-widget-controls {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin: 0 0 8px 0;
}

.figure-widget-control-label {
  font-size: 14px;
}

.figure-widget-segmented {
  align-items: center;
  display: inline-flex;
  gap: 6px;
}

.figure-widget-segmented button {
  background: #fff;
  border: 1px solid #aaa;
  border-radius: 4px;
  color: #111;
  cursor: pointer;
  font: inherit;
  padding: 4px 8px;
}

.figure-widget-segmented button.is-active {
  background: #111;
  border-color: #111;
  color: #fff;
}

.figure-widget-slider {
  align-items: center;
  display: flex;
  flex: 1 1 260px;
  gap: 8px;
  min-width: 260px;
}

.figure-widget-slider label {
  font-size: 14px;
  white-space: nowrap;
}

.figure-widget-slider input {
  flex: 1;
  min-width: 120px;
}

.figure-widget-slider-value {
  font-variant-numeric: tabular-nums;
  min-width: 3em;
}

.figure-widget-stage {
  max-width: 100%;
}

.figure-widget-svg {
  background: #fafafa;
  height: auto;
  max-width: 100%;
  touch-action: none;
  user-select: none;
}

.figure-point-handle {
  cursor: grab;
}

.figure-point-handle:hover,
.figure-point-handle.is-dragging {
  fill: #fff3bf;
}
`;
  (document.getElementsByTagName('head')[0] || document.head).appendChild(style);
}

// Ordered hash — keeps insertion order over an arbitrary key
// type, with a `sortedKeys()` helper for picking the
// largest-by-numeric-key entry. Used by `convex_hull_farthest_point`
// to map projected distances → farthest points; the entry with the
// largest key is the answer. Predates ES6 `Map` (which preserves
// insertion order natively); a future cleanup could collapse this
// onto `Map` + a sort, but the surface is stable so there's no
// forcing function.
class OrderedHash {
  constructor() {
    this._keys = [];
    this.vals = {};
  }

  push(k, v) {
    if (!this.vals[k]) this._keys.push(k);
    this.vals[k] = v;
  }

  insert(pos, k, v) {
    if (!this.vals[k]) {
      this._keys.splice(pos, 0, k);
      this.vals[k] = v;
    }
  }

  get(k) {
    return this.vals[k];
  }

  length() {
    return this._keys.length;
  }

  keys() {
    return this._keys;
  }

  sortedKeys() {
    return this.keys().sort((a, b) => a - b);
  }

  values() {
    return this.vals;
  }
}

// 2D vector. Pure math — no DOM, no allocation hot paths to worry
// about (widgets are single-instance and re-rendered on user input,
// not in tight loops). Kept immutable: every operation returns a
// new Vector rather than mutating `this`. Static constants
// `Vector.null`, `Vector.up`, `Vector.right` are populated below
// the class declaration (you can't reference the class name from
// inside a static-field initializer).
class Vector {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }

  length() {
    return Math.sqrt(this.x * this.x + this.y * this.y);
  }

  plus(vector) {
    return new Vector(this.x + vector.x, this.y + vector.y);
  }

  minus(vector) {
    return new Vector(this.x - vector.x, this.y - vector.y);
  }

  multiply(scalar) {
    return new Vector(this.x * scalar, this.y * scalar);
  }

  dot(vector) {
    return this.x * vector.x + this.y * vector.y;
  }

  normalized() {
    return this.multiply(1.0 / this.length());
  }

  rotated(angle) {
    return new Vector(
      this.x * Math.cos(angle) - this.y * Math.sin(angle),
      this.x * Math.sin(angle) + this.y * Math.cos(angle)
    );
  }
}

Vector.null = new Vector(0, 0);
Vector.up = new Vector(0, -1);     // y-axis points DOWN in SVG, so "up" is negative y.
Vector.right = new Vector(1, 0);

const svgns = 'http://www.w3.org/2000/svg';

// SVG canvas — the outer `<svg>` element a widget renders into.
// Default transform translates origin to bottom-left and scales to
// 30 SVG units per scene unit, so widget code can think in scene
// coordinates throughout. Override via `setTransform` for atypical
// layouts.
class Canvas {
  constructor(width, height) {
    this.width = width;
    this.height = height;
    this.elements = [];
    this.transform = `translate(0, ${height}) scale(30, 30)`;
  }

  add(element) {
    this.elements.push(element);
  }

  setTransform(transform) {
    this.transform = transform;
  }

  translate(vector) {
    this.transform += ` translate(${vector.x}, ${vector.y})`;
  }

  center() {
    this.translate(new Vector(5.5, -4));
  }

  toSVG() {
    const element = document.createElementNS(svgns, 'svg');
    element.setAttribute('width', this.width);
    element.setAttribute('height', this.height);
    element.setAttribute('class', 'figure-canvas');

    const defs = document.createElementNS(svgns, 'defs');
    defs.innerHTML = `
    <marker id="arrow" markerWidth="10" markerHeight="10" refx="8" refy="3" orient="auto" markerUnits="strokeWidth">
      <path d="M0,0 L0,6 L9,3 z" fill="#000" />
    </marker>
    `;
    element.appendChild(defs);

    const group = document.createElementNS(svgns, 'g');
    group.setAttribute('transform', this.transform);

    for (const e of this.elements) {
      group.appendChild(e.toSVG());
    }

    element.appendChild(group);
    return element;
  }
}

// SVG group — collects child elements under a shared transform.
// Use to apply a rotation / translation to a set of figures
// without mutating the canvas's transform stack.
class Group {
  constructor() {
    this.elements = [];
    this.transform = '';
  }

  add(element) {
    this.elements.push(element);
  }

  setTransform(transform) {
    this.transform = transform;
  }

  toSVG() {
    const group = document.createElementNS(svgns, 'g');
    group.setAttribute('transform', this.transform);
    for (const e of this.elements) {
      group.appendChild(e.toSVG());
    }
    return group;
  }
}

// Single-segment line from origin to origin+direction. CSS class
// `arrow` adds an arrowhead at the end via the marker defined in
// `Canvas#toSVG`'s `<defs>`.
class Line {
  constructor(origin, direction, klass) {
    this.origin = origin;
    this.direction = direction;
    this.klass = klass;
  }

  toSVG() {
    const line = document.createElementNS(svgns, 'line');
    const end = this.origin.plus(this.direction);
    line.setAttribute('x1', this.origin.x);
    line.setAttribute('y1', this.origin.y);
    line.setAttribute('x2', end.x);
    line.setAttribute('y2', end.y);
    line.setAttribute('class', this.klass);
    return line;
  }
}

// A directional ray drawn as an arrow + the rest of the line
// extended. Pass `both: true` for a bidirectional line.
class Ray {
  constructor(origin, direction, both) {
    this.origin = origin;
    this.direction = direction;
    this.both = both;
  }

  toSVG() {
    const group = new Group();
    group.add(new Line(this.origin, this.direction, 'arrow'));
    if (this.both) {
      group.add(new Line(this.at(-50), this.direction.multiply(100)));
    } else {
      group.add(new Line(this.origin, this.direction.multiply(100)));
    }
    return group.toSVG();
  }

  at(distance) {
    return this.origin.plus(this.direction.multiply(distance));
  }

  projectedDistance(vector) {
    return this.direction.dot(vector.minus(this.origin)) /
           this.direction.dot(this.direction);
  }

  projected(vector) {
    return this.at(this.projectedDistance(vector));
  }
}

// Circle outline. CSS classes: `intersection` (filled black, used
// to mark a single point), `result` (filled red, used to mark a
// computed result the widget is illustrating).
class Circle {
  constructor(center, radius, klass) {
    this.center = center;
    this.radius = radius;
    this.klass = klass;
  }

  toSVG() {
    const circle = document.createElementNS(svgns, 'circle');
    circle.setAttribute('cx', this.center.x);
    circle.setAttribute('cy', this.center.y);
    circle.setAttribute('r', this.radius);
    circle.setAttribute('class', this.klass);
    return circle;
  }
}

// Rectangle outline. CSS class `dashed` for dashed strokes.
class Rectangle {
  constructor(topleft, size, klass) {
    this.topleft = topleft;
    this.size = size;
    this.klass = klass;
  }

  toSVG() {
    const rectangle = document.createElementNS(svgns, 'rect');
    rectangle.setAttribute('x', this.topleft.x);
    rectangle.setAttribute('y', this.topleft.y);
    rectangle.setAttribute('width', this.size.x);
    rectangle.setAttribute('height', this.size.y);
    rectangle.setAttribute('class', this.klass);
    return rectangle;
  }
}

// Text label.
class Text {
  constructor(position, text, klass) {
    this.position = position;
    this.text = text;
    this.klass = klass;
  }

  toSVG() {
    const text = document.createElementNS(svgns, 'text');
    text.setAttribute('x', this.position.x);
    text.setAttribute('y', this.position.y);
    text.setAttribute('class', this.klass);
    text.innerHTML = this.text;
    return text;
  }
}

// x/y axes with arrowheads + "x"/"y" labels at the tips. Used by
// most math-illustration widgets to give the viewer a frame of
// reference.
class Axes {
  constructor(length) {
    this.origin = Vector.null;
    this.length = length || 3;
  }

  toSVG() {
    const group = new Group();
    group.add(new Line(this.origin, new Vector(this.length, 0), 'axis'));
    group.add(new Line(this.origin, new Vector(0, -this.length), 'axis'));
    group.add(new Text(new Vector(this.length, 0.4), 'x'));
    group.add(new Text(new Vector(-0.4, -this.length), 'y'));
    return group.toSVG();
  }
}

// Arbitrary SVG path. Use for curves, polygons, function plots —
// anything not expressible as line/circle/rect. Use `Path.polyline`
// to build a straight-segment d-string from a `Vector[]`. See the
// MDN "SVG path" reference for the full d-string syntax.
class Path {
  constructor(d, klass) {
    this.d = d;
    this.klass = klass;
  }

  toSVG() {
    const path = document.createElementNS(svgns, 'path');
    path.setAttribute('d', this.d);
    if (this.klass) path.setAttribute('class', this.klass);
    path.setAttribute('fill', 'transparent');
    return path;
  }

  // Build a d-string from a list of points connecting them with
  // line-to commands. `{closed: true}` appends a Z to close the
  // polygon.
  static polyline(points, opts = {}) {
    if (points.length === 0) return '';
    let d = `M ${points[0].x} ${points[0].y}`;
    for (let i = 1; i < points.length; i++) {
      d += ` L ${points[i].x} ${points[i].y}`;
    }
    if (opts.closed) d += ' Z';
    return d;
  }
}

// HTML range slider with a live label. Use this for parameter
// widgets where the user benefits from a known affordance, a
// defined range, and a numeric readout. Returns a `<div>` from
// `.element()`; place anywhere in the DOM (above the SVG canvas
// is the conventional placement).
class Slider {
  constructor({ label = '', min, max, step, value, precision = 2, onChange = () => {} }) {
    this.label = label;
    this.min = min;
    this.max = max;
    this.step = step !== undefined ? step : (max - min) / 100.0;
    this.value = value !== undefined ? value : (min + max) / 2.0;
    this.precision = precision;
    this.onChange = onChange;
  }

  element() {
    const div = document.createElement('div');
    Object.assign(div.style, {
      fontFamily: 'sans-serif',
      fontSize: '13px',
      padding: '4px 0',
      display: 'flex',
      alignItems: 'center',
      gap: '8px'
    });

    const label = document.createElement('label');
    label.textContent = `${this.label} = ${this.value.toFixed(this.precision)}`;
    label.style.minWidth = '12em';

    const input = document.createElement('input');
    input.type = 'range';
    input.min = this.min;
    input.max = this.max;
    input.step = this.step;
    input.value = this.value;
    input.style.flex = '1';

    input.addEventListener('input', (e) => {
      const v = parseFloat(e.target.value);
      label.textContent = `${this.label} = ${v.toFixed(this.precision)}`;
      this.onChange(v);
    });

    div.appendChild(label);
    div.appendChild(input);
    return div;
  }
}

function setAttributes(element, attrs = {}) {
  for (const [key, value] of Object.entries(attrs)) {
    if (value === undefined || value === null) continue;
    if (key === 'style' && typeof value === 'object') {
      Object.assign(element.style, value);
    } else if (key === 'textContent') {
      element.textContent = value;
    } else {
      element.setAttribute(key, value);
    }
  }
  return element;
}

function createSvgElement(name, attrs = {}) {
  return setAttributes(document.createElementNS(svgns, name), attrs);
}

class FigureWidget {
  constructor({ className = '' } = {}) {
    this.root = document.createElement('div');
    this.root.className = ['figure-widget', className].filter(Boolean).join(' ');
    this.controls = document.createElement('div');
    this.controls.className = 'figure-widget-controls';
    this.stage = document.createElement('div');
    this.stage.className = 'figure-widget-stage';
    this.root.appendChild(this.controls);
    this.root.appendChild(this.stage);
  }

  addControl(element) {
    this.controls.appendChild(element);
    return element;
  }

  setControlsVisible(visible) {
    this.controls.style.display = visible ? 'flex' : 'none';
  }

  setContent(element) {
    while (this.stage.children.length > 0) {
      this.stage.removeChild(this.stage.children[0]);
    }
    this.stage.appendChild(element);
  }

  mount(scriptElement) {
    scriptElement.parentNode.appendChild(this.root);
  }
}

class FigureSvg {
  constructor({ width, height, viewBox, className = '', background = '#fafafa' }) {
    this.width = width;
    this.height = height;
    this.element = createSvgElement('svg', {
      width,
      height,
      viewBox: viewBox || `0 0 ${width} ${height}`,
      class: ['figure-widget-svg', className].filter(Boolean).join(' '),
      style: `background: ${background};`,
    });
  }

  clear() {
    while (this.element.children.length > 0) {
      this.element.removeChild(this.element.children[0]);
    }
  }

  add(name, attrs = {}) {
    const element = createSvgElement(name, attrs);
    this.element.appendChild(element);
    return element;
  }

  append(element) {
    this.element.appendChild(element);
    return element;
  }

  pointFromEvent(event) {
    return FigureSvg.pointFromEvent(this.element, event);
  }

  static pointFromEvent(svg, event) {
    if (svg.getBoundingClientRect) {
      const rect = svg.getBoundingClientRect();
      if (rect.width > 0 && rect.height > 0) {
        return FigureSvg.pointFromRect(svg, event, rect);
      }
    }

    if (svg.createSVGPoint) {
      const point = svg.createSVGPoint();
      point.x = event.clientX;
      point.y = event.clientY;
      const matrix = svg.getScreenCTM();
      if (matrix) {
        const local = point.matrixTransform(matrix.inverse());
        return { x: local.x, y: local.y };
      }
    }

    const rect = svg.getBoundingClientRect();
    const [minX, minY, width, height] = svg.getAttribute('viewBox')
      .split(/\s+/).map(Number);
    return {
      x: minX + ((event.clientX - rect.left) / rect.width) * width,
      y: minY + ((event.clientY - rect.top) / rect.height) * height,
    };
  }

  static pointFromRect(svg, event, rect) {
    const [minX, minY, viewBoxWidth, viewBoxHeight] = svg.getAttribute('viewBox')
      .split(/\s+/).map(Number);
    const preserve = (svg.getAttribute('preserveAspectRatio') || 'xMidYMid meet')
      .trim().split(/\s+/).filter(token => token !== 'defer');
    const align = preserve[0] || 'xMidYMid';
    const meetOrSlice = preserve[1] || 'meet';
    const clientX = event.clientX - rect.left;
    const clientY = event.clientY - rect.top;

    if (align === 'none') {
      return {
        x: minX + (clientX / rect.width) * viewBoxWidth,
        y: minY + (clientY / rect.height) * viewBoxHeight,
      };
    }

    const scaleX = rect.width / viewBoxWidth;
    const scaleY = rect.height / viewBoxHeight;
    const scale = meetOrSlice === 'slice'
      ? Math.max(scaleX, scaleY)
      : Math.min(scaleX, scaleY);
    const contentWidth = viewBoxWidth * scale;
    const contentHeight = viewBoxHeight * scale;
    let offsetX = 0;
    let offsetY = 0;
    if (align.includes('xMid')) offsetX = (rect.width - contentWidth) / 2;
    if (align.includes('xMax')) offsetX = rect.width - contentWidth;
    if (align.includes('YMid')) offsetY = (rect.height - contentHeight) / 2;
    if (align.includes('YMax')) offsetY = rect.height - contentHeight;

    return {
      x: minX + (clientX - offsetX) / scale,
      y: minY + (clientY - offsetY) / scale,
    };
  }
}

class FigureSegmentedControl {
  constructor({ label = '', options = [], value, onChange = () => {} }) {
    this.label = label;
    this.options = options;
    this.value = value;
    this.onChange = onChange;
    this.buttons = [];
  }

  element() {
    if (this.root) return this.root;

    this.root = document.createElement('div');
    this.root.className = 'figure-widget-segmented';

    if (this.label) {
      const label = document.createElement('span');
      label.className = 'figure-widget-control-label';
      label.textContent = this.label;
      this.root.appendChild(label);
    }

    this.buttons = this.options.map((option) => {
      const button = document.createElement('button');
      button.type = 'button';
      button.textContent = option.label;
      button.setAttribute('data-value', option.value);
      button.addEventListener('click', () => {
        this.update(option.value);
        this.onChange(option.value);
      });
      this.root.appendChild(button);
      return button;
    });

    this.update(this.value);
    return this.root;
  }

  update(value) {
    this.value = value;
    this.buttons.forEach((button) => {
      const active = button.getAttribute('data-value') === String(value);
      button.className = active ? 'is-active' : '';
    });
  }
}

class FigureSliderControl {
  constructor({
    label = '',
    min,
    max,
    step,
    value,
    precision = 2,
    format = null,
    onChange = () => {},
  }) {
    this.label = label;
    this.min = min;
    this.max = max;
    this.step = step !== undefined ? step : (max - min) / 100.0;
    this.value = value !== undefined ? value : (min + max) / 2.0;
    this.precision = precision;
    this.format = format || (v => v.toFixed(this.precision));
    this.onChange = onChange;
  }

  element() {
    if (this.root) return this.root;

    this.root = document.createElement('div');
    this.root.className = 'figure-widget-slider';

    this.labelElement = document.createElement('label');
    this.labelElement.textContent = this.label;
    this.root.appendChild(this.labelElement);

    this.input = document.createElement('input');
    this.input.type = 'range';
    this.input.min = this.min;
    this.input.max = this.max;
    this.input.step = this.step;
    this.input.value = this.value;
    this.input.addEventListener('input', (event) => {
      this.update(parseFloat(event.target.value), true);
    });
    this.root.appendChild(this.input);

    this.valueElement = document.createElement('span');
    this.valueElement.className = 'figure-widget-slider-value';
    this.root.appendChild(this.valueElement);

    this.update(this.value, false);
    return this.root;
  }

  update(value, notify = false) {
    this.value = value;
    if (this.input) this.input.value = value;
    if (this.valueElement) this.valueElement.textContent = this.format(value);
    if (notify) this.onChange(value);
  }
}

class FigureDraggablePoint {
  constructor({ canvas = null, svg = null, point, radius = 8, className = '', attrs = {}, onDrag = () => {} }) {
    this.canvas = canvas;
    this.svg = svg || (canvas && canvas.element);
    this.point = point;
    this.radius = radius;
    this.className = className;
    this.attrs = attrs;
    this.onDrag = onDrag;
  }

  element() {
    const handle = createSvgElement('circle', {
      cx: this.point.x,
      cy: this.point.y,
      r: this.radius,
      class: ['figure-point-handle', this.className].filter(Boolean).join(' '),
      tabindex: 0,
      ...this.attrs,
    });

    handle.addEventListener('pointerdown', (event) => {
      const start = this.pointFromEvent(event);
      const dragOffset = {
        x: this.point.x - start.x,
        y: this.point.y - start.y,
      };
      const move = (moveEvent) => {
        handle.setAttribute('class', ['figure-point-handle', 'is-dragging', this.className]
          .filter(Boolean).join(' '));
        const point = this.pointFromEvent(moveEvent);
        this.onDrag({
          x: point.x + dragOffset.x,
          y: point.y + dragOffset.y,
        }, moveEvent);
        moveEvent.preventDefault();
      };
      const up = () => {
        handle.setAttribute('class', ['figure-point-handle', this.className].filter(Boolean).join(' '));
        document.removeEventListener('pointermove', move);
        document.removeEventListener('pointerup', up);
      };
      document.addEventListener('pointermove', move);
      document.addEventListener('pointerup', up);
      move(event);
      event.preventDefault();
    });

    return handle;
  }

  pointFromEvent(event) {
    return this.canvas ? this.canvas.pointFromEvent(event) : FigureSvg.pointFromEvent(this.svg, event);
  }
}

// Mouse-drag affordance. Wraps a figure object that has a
// `createCanvas()` method and a per-drag handler. The handler
// receives the (delta, figure) and returns true to redraw the
// canvas. Lower-discoverability than `Slider` for documentation
// pages — prefer Slider unless the parameter is genuinely 2D
// (e.g. dragging a point in the plane).
class DragHandler {
  constructor(figure) {
    this.figure = figure;
    this.handlerFunc = () => false;
  }

  divElement() {
    if (this.element) return this.element;

    this.element = document.createElement('div');

    let mousex = null;
    let mousey = null;

    this.element.addEventListener('mousedown', (e) => {
      mousex = e.pageX;
      mousey = e.pageY;

      document.onmousemove = (event) => {
        event = event || window.event;
        const delta = new Vector(event.pageX - mousex, event.pageY - mousey);
        if (this.handlerFunc(delta, this.figure)) {
          const newCanvas = this.figure.createCanvas();
          this.element.replaceChild(newCanvas, this.canvas);
          this.canvas = newCanvas;
          event.stopPropagation();
          event.preventDefault();
        }
        mousex = event.pageX;
        mousey = event.pageY;
      };

      document.onmouseup = () => {
        document.onmousemove = null;
        mousex = null;
        mousey = null;
      };

      e.stopPropagation();
      e.preventDefault();
    });

    this.canvas = this.figure.createCanvas();
    this.canvas.onselectstart = () => false;
    this.canvas.unselectable = 'on';

    this.element.appendChild(this.canvas);
    this.element.unselectable = 'on';
    this.element.onselectstart = () => false;
    this.element.style.userSelect = 'none';

    return this.element;
  }
}

// Conversion factor: degrees → radians. Used by widgets that take
// angle inputs in degrees for clarity (e.g. `12 * degrees`).
const degrees = 0.01745329251996;

// Export every primitive to the global scope. ES6 `class`
// declarations are block-scoped (per spec, even at script top
// level), so without this the widgets' `new Vector(...)` /
// `new Canvas(...)` references can't resolve across script-tag
// boundaries. Browsers and the `node:test` shim both treat the
// script as a top-level script; `globalThis` is the document
// window in browsers and the vm sandbox in tests.
Object.assign(globalThis, {
  OrderedHash, Vector, Canvas, Group, Line, Ray, Circle,
  Rectangle, Text, Axes, Path, Slider, setAttributes, createSvgElement,
  FigureWidget, FigureSvg, FigureSegmentedControl,
  FigureSliderControl, FigureDraggablePoint, DragHandler, svgns, degrees,
  FigureStrokeWidth, FigureGuideStrokeWidth,
  FigurePixelStrokeWidth, FigurePixelGuideStrokeWidth
});
