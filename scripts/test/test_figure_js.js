// Tests for the math primitives in scripts/docs/figure.js.
//
// figure.js is loaded as a browser script, with side effects: it
// injects CSS into document.head and uses document.createElementNS
// for SVG output. None of that survives in a Node test runner, so
// we install a minimal DOM shim, define a global `document` object
// against which figure.js's top-level mutations succeed, then load
// the file via Node's vm module to evaluate it in our shim's
// context. After that, `Vector`, `Class`, etc. are accessible on
// the shim's globals.
//
// Tests focus on the pure-math primitives (Vector arithmetic,
// Class factory) and the structural correctness of SVG-emitting
// primitives (Path's `d` attribute, Slider's HTML structure). The
// rendered SVG output itself is integration-tested by the Doxygen
// browser preview — there's no point reproducing a full SVG
// renderer in Node.
//
// Run: `node scripts/test/test_figure_js.js`
//      (or `rake test:scripts:js`)

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');

// --- Minimal DOM shim ------------------------------------------------------
//
// Just enough of the DOM for figure.js to load: createElement /
// createElementNS, setAttribute, appendChild, innerHTML setter,
// addEventListener no-op. Nodes track their tag, attributes, and
// children so we can introspect the produced trees in assertions.

function makeNode(tag, ns) {
  const node = {
    tagName: tag,
    nodeType: 1,
    namespace: ns || null,
    attributes: {},
    children: [],
    parentNode: null,
    style: {},
    _innerHTML: '',
    setAttribute(key, value) { this.attributes[key] = String(value); },
    getAttribute(key) { return this.attributes[key]; },
    appendChild(child) {
      child.parentNode = this;
      this.children.push(child);
      return child;
    },
    replaceChild(newNode, oldNode) {
      const i = this.children.indexOf(oldNode);
      if (i === -1) throw new Error('replaceChild: oldNode not a child');
      this.children[i] = newNode;
      newNode.parentNode = this;
      oldNode.parentNode = null;
      return oldNode;
    },
    addEventListener() {},
    removeEventListener() {},
    get innerHTML() { return this._innerHTML; },
    set innerHTML(v) { this._innerHTML = v; }
  };
  Object.defineProperty(node, 'textContent', {
    get() { return this._textContent || ''; },
    set(v) { this._textContent = v; }
  });
  return node;
}

function makeDocument() {
  const head = makeNode('head');
  const html = makeNode('html');
  html.appendChild(head);
  const doc = {
    scripts: [],
    currentScript: null,
    createElement(tag) { return makeNode(tag); },
    createElementNS(ns, tag) { return makeNode(tag, ns); },
    getElementsByTagName(tag) {
      if (tag === 'head') return [head];
      return [];
    }
  };
  return doc;
}

// --- Load figure.js into the shim ------------------------------------------

function loadFigure() {
  const figurePath = path.resolve(__dirname, '..', 'docs', 'figure.js');
  const source = fs.readFileSync(figurePath, 'utf8');
  const sandbox = {
    document: makeDocument(),
    window: {},
    Math: Math,
    console: console
  };
  // Make `window` reference itself the way browsers do, in case a
  // later figure.js change starts using it.
  sandbox.window = sandbox;
  vm.createContext(sandbox);
  vm.runInContext(source, sandbox);
  return sandbox;
}

// --- Vector tests ----------------------------------------------------------

test('Vector: arithmetic produces the right components', () => {
  const { Vector } = loadFigure();
  const a = new Vector(3, 4);
  assert.equal(a.x, 3);
  assert.equal(a.y, 4);

  const b = new Vector(1, 2);
  assert.deepEqual({ x: a.plus(b).x, y: a.plus(b).y }, { x: 4, y: 6 });
  assert.deepEqual({ x: a.minus(b).x, y: a.minus(b).y }, { x: 2, y: 2 });
  assert.deepEqual({ x: a.multiply(2).x, y: a.multiply(2).y }, { x: 6, y: 8 });
});

test('Vector: length is the Euclidean norm', () => {
  const { Vector } = loadFigure();
  assert.equal(new Vector(3, 4).length(), 5);
  assert.equal(new Vector(0, 0).length(), 0);
  assert.equal(new Vector(1, 0).length(), 1);
});

test('Vector: dot product matches the math', () => {
  const { Vector } = loadFigure();
  assert.equal(new Vector(1, 0).dot(new Vector(0, 1)), 0,
    'orthogonal vectors → dot product 0');
  assert.equal(new Vector(2, 3).dot(new Vector(4, 5)), 23);
});

test('Vector: normalized has unit length', () => {
  const { Vector } = loadFigure();
  const n = new Vector(3, 4).normalized();
  assert.ok(Math.abs(n.length() - 1) < 1e-12,
    `expected unit length, got ${n.length()}`);
});

test('Vector: rotated by 90° swaps coordinates correctly', () => {
  const { Vector } = loadFigure();
  // `rotated(angle)` is in radians. 90° = π/2. Rotating (1, 0) by
  // π/2 should give (0, 1) within float precision.
  const r = new Vector(1, 0).rotated(Math.PI / 2);
  assert.ok(Math.abs(r.x) < 1e-12, `x ≈ 0, got ${r.x}`);
  assert.ok(Math.abs(r.y - 1) < 1e-12, `y ≈ 1, got ${r.y}`);
});

test('Vector: static constants', () => {
  const { Vector } = loadFigure();
  assert.deepEqual({ x: Vector.null.x, y: Vector.null.y }, { x: 0, y: 0 });
  assert.deepEqual({ x: Vector.up.x, y: Vector.up.y }, { x: 0, y: -1 },
    'Vector.up is (0, -1) — y-axis points DOWN in SVG so "up" is negative');
  assert.deepEqual({ x: Vector.right.x, y: Vector.right.y }, { x: 1, y: 0 });
});

// --- Class factory tests ---------------------------------------------------

test('Class: instances run their initialize', () => {
  const { Class } = loadFigure();
  const Foo = new Class({
    initialize(x) { this.x = x; }
  });
  assert.equal(new Foo(42).x, 42);
});

test('Class: subclass inherits methods from parent', () => {
  const { Class } = loadFigure();
  const Animal = new Class({
    initialize(name) { this.name = name; },
    describe() { return 'an animal called ' + this.name; }
  });
  const Dog = new Class(Animal, {
    initialize(name) { this.name = name; this.species = 'dog'; },
    bark() { return 'woof'; }
  });
  const d = new Dog('Rex');
  assert.equal(d.name, 'Rex');
  assert.equal(d.bark(), 'woof');
  assert.equal(d.describe(), 'an animal called Rex',
    'subclass should inherit `describe` from parent');
});

test('Class: empty class still constructs', () => {
  const { Class } = loadFigure();
  const Empty = new Class({});
  assert.doesNotThrow(() => new Empty());
});

// --- Path primitive --------------------------------------------------------

test('Path: emits a <path> element with the supplied d attribute', () => {
  const { Path } = loadFigure();
  const svg = new Path('M 0 0 L 1 1', 'result').toSVG();
  assert.equal(svg.tagName, 'path');
  assert.equal(svg.getAttribute('d'), 'M 0 0 L 1 1');
  assert.equal(svg.getAttribute('class'), 'result');
});

test('Path: omits class attribute when no klass given', () => {
  const { Path } = loadFigure();
  const svg = new Path('M 0 0 L 1 1').toSVG();
  assert.equal(svg.attributes['class'], undefined,
    'no klass passed → no class attribute (rather than class="undefined")');
});

test('Path.polyline: builds a connected polyline d string', () => {
  const { Path, Vector } = loadFigure();
  const d = Path.polyline([new Vector(0, 0), new Vector(1, 0), new Vector(1, 1)]);
  assert.equal(d, 'M 0 0 L 1 0 L 1 1');
});

test('Path.polyline: closed: true appends Z', () => {
  const { Path, Vector } = loadFigure();
  const d = Path.polyline([new Vector(0, 0), new Vector(1, 0), new Vector(1, 1)],
                          { closed: true });
  assert.equal(d, 'M 0 0 L 1 0 L 1 1 Z');
});

test('Path.polyline: empty array returns empty string', () => {
  const { Path } = loadFigure();
  assert.equal(Path.polyline([]), '');
});

// --- Slider primitive ------------------------------------------------------

test('Slider: builds a label + range input with the right attributes', () => {
  const { Slider } = loadFigure();
  const slider = new Slider({
    label: 'focalDistance', min: 1, max: 7, value: 4, step: 0.5
  });
  const div = slider.element();

  assert.equal(div.tagName, 'div');
  assert.equal(div.children.length, 2, 'div contains label + input');

  const [label, input] = div.children;
  assert.equal(label.tagName, 'label');
  assert.match(label.textContent, /^focalDistance =/);

  // figure.js sets `input.type` as a property (the canonical
  // browser-API way), not via setAttribute — both work in real
  // browsers but the property version is more direct.
  assert.equal(input.tagName, 'input');
  assert.equal(input.type, 'range');
  assert.equal(input.min, 1);
  assert.equal(input.max, 7);
  assert.equal(input.value, 4);
  assert.equal(input.step, 0.5);
});

test('Slider: precision: 0 produces integer labels', () => {
  const { Slider } = loadFigure();
  const slider = new Slider({ label: 'n', min: 2, max: 10, value: 6, precision: 0 });
  const div = slider.element();
  assert.equal(div.children[0].textContent, 'n = 6');
});

test('Slider: precision: 2 produces two-decimal labels', () => {
  const { Slider } = loadFigure();
  const slider = new Slider({ label: 'x', min: 0, max: 1, value: 0.123, precision: 2 });
  const div = slider.element();
  assert.equal(div.children[0].textContent, 'x = 0.12');
});

test('Slider: defaults value to midpoint when not supplied', () => {
  const { Slider } = loadFigure();
  const slider = new Slider({ label: 'x', min: 0, max: 10 });
  assert.equal(slider.value, 5);
});

test('Slider: defaults step to 1/100th of range', () => {
  const { Slider } = loadFigure();
  const slider = new Slider({ label: 'x', min: 0, max: 100 });
  assert.equal(slider.step, 1);
});
