// Interactive-widget primitives shared by every `scripts/docs/*.js`
// widget embedded in the Doxygen output. See `scripts/README.md` for
// the writing-a-widget recipe and `scripts/test/test_figure_js.js`
// for the test cases.
//
// Two coexisting styles for declaring primitives:
//
//   - **Native ES6 classes** (`class Foo { ... }`) — the recommended
//     style for new widgets. Modern, terser, plays well with editor
//     tooling and the `node:test` runner.
//
//   - **Legacy `Class()` factory** — preserved as a thin shim around
//     ES6 classes for backward compatibility with the older widgets
//     under `scripts/docs/` (Angle, Ray, BoundingBox, ConvexHull,
//     ...) that all use `new Class({...})` syntax. Migrate them
//     opportunistically when next touched.

'use strict';

// CSS injection — runs at script-load time. Wrapped so it no-ops in
// non-browser environments (Node test runner): the test shim
// provides a `document` but doesn't simulate `<style>` parsing.
if (typeof document !== 'undefined' && document.head) {
  const style = document.createElement('style');
  style.type = 'text/css';
  style.innerHTML = `
svg * {
  stroke-width: 0.033;
}

svg .dashed {
  stroke-dasharray: 0.1, 0.1;
}

svg .red {
  stroke: #ff0000;
}

svg .red marker {
  stroke: #ff0000;
}

text {
  font-size: 3.3%;
}

line {
  stroke: #000000;
}

line.arrow {
  marker-end: url(#arrow);
}

line.axis {
  stroke-width: 0.05;
  marker-end: url(#arrow);
}

circle {
  stroke: #000000;
  fill: transparent;
}

circle.intersection {
  stroke: #000000;
  fill: #000000;
}

circle.result {
  stroke: #ff0000;
  fill: #ff0000;
}

rect {
  stroke: #000000;
  fill: transparent;
}
`;
  (document.getElementsByTagName('head')[0] || document.head).appendChild(style);
}

// Legacy-compat factory. Original widgets use `new Class({...})` and
// `new Class(parent, {...})`; preserve that surface by translating
// to a real ES6 class under the hood. Mark new code as preferring
// `class Foo extends Bar { ... }` directly; reach for this only when
// touching pre-existing consumers that already used it.
function Class(...args) {
  let parent = null;
  let properties = {};
  if (typeof args[0] === 'function') {
    parent = args[0];
    properties = args[1] || {};
  } else {
    properties = args[0] || {};
  }

  const Base = parent || class {};
  const klass = class extends Base {
    constructor(...ctorArgs) {
      super();
      if (typeof this.initialize === 'function') {
        this.initialize(...ctorArgs);
      }
    }
  };

  for (const property in properties) {
    klass.prototype[property] = properties[property];
  }
  if (!klass.prototype.initialize) {
    klass.prototype.initialize = function () {};
  }
  return klass;
}

// Ordered hash. Used by a couple of legacy widgets; kept for
// compatibility. New widgets should reach for `Map` instead.
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
// level), so without this the legacy widgets' `new Vector(...)` /
// `new Class(...)` references can't resolve them. Browsers and the
// `node:test` shim both treat the script as a top-level script;
// `globalThis` is the document window in browsers and the vm
// sandbox in tests.
Object.assign(globalThis, {
  Class, OrderedHash, Vector, Canvas, Group, Line, Ray, Circle,
  Rectangle, Text, Axes, Path, Slider, DragHandler, svgns, degrees
});
