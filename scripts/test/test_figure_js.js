// Tests for the math + SVG primitives in scripts/docs/figure.js,
// plus a smoke test that loads every widget under scripts/docs/.
//
// figure.js is loaded as a browser script, with side effects: it
// injects CSS into document.head and uses document.createElementNS
// for SVG output. None of that survives in a Node test runner, so
// we install a minimal DOM shim, define a global `document` object
// against which figure.js's top-level mutations succeed, then load
// the file via Node's vm module to evaluate it in our shim's
// context. After that, `Vector`, `Canvas`, etc. are accessible on
// the shim's globals.
//
// Tests focus on the pure-math primitives (Vector arithmetic) and
// the structural correctness of SVG/HTML-emitting primitives
// (Path's `d` attribute, Slider's HTML structure). The rendered
// SVG output itself is integration-tested by the Doxygen browser
// preview — there's no point reproducing a full SVG renderer in
// Node.
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
    listeners: {},
    _innerHTML: '',
    setAttribute(key, value) { this.attributes[key] = String(value); },
    getAttribute(key) { return this.attributes[key]; },
    appendChild(child) {
      child.parentNode = this;
      this.children.push(child);
      return child;
    },
    removeChild(child) {
      const i = this.children.indexOf(child);
      if (i === -1) throw new Error('removeChild: node not a child');
      this.children.splice(i, 1);
      child.parentNode = null;
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
    addEventListener(type, handler) {
      this.listeners[type] = this.listeners[type] || [];
      this.listeners[type].push(handler);
    },
    removeEventListener() {},
    dispatchEvent(event) {
      (this.listeners[event.type] || []).forEach(handler => handler(event));
    },
    click() { this.dispatchEvent({ type: 'click', target: this }); },
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

function loadWidget(widget) {
  return loadWidgets([widget]);
}

function loadWidgets(widgets) {
  const sandbox = loadFigure();
  const body = sandbox.document.createElement('body');
  const scripts = [];
  widgets.forEach((widget) => {
    const script = sandbox.document.createElement('script');
    body.appendChild(script);
    scripts.push(script);
    sandbox.document.currentScript = script;
    sandbox.document.scripts = scripts;

    const source = fs.readFileSync(path.resolve(__dirname, '..', 'docs', widget), 'utf8');
    vm.runInContext(source, sandbox);
  });
  return body;
}

function countElements(node, tagName) {
  const self = node.tagName === tagName ? 1 : 0;
  return self + node.children.reduce((sum, child) => sum + countElements(child, tagName), 0);
}

function elementsByTag(node, tagName) {
  const self = node.tagName === tagName ? [node] : [];
  return self.concat(node.children.flatMap(child => elementsByTag(child, tagName)));
}

function textContents(node) {
  const own = node.textContent ? [node.textContent] : (node.innerHTML ? [node.innerHTML] : []);
  return own.concat(node.children.flatMap(textContents));
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

test('Figure stroke constants: standard weights are exported', () => {
  const {
    FigureStrokeWidth,
    FigureGuideStrokeWidth,
    FigurePixelStrokeWidth,
    FigurePixelGuideStrokeWidth,
  } = loadFigure();
  assert.equal(FigureStrokeWidth, 0.05);
  assert.equal(FigureGuideStrokeWidth, 0.035);
  assert.equal(FigurePixelStrokeWidth, 2);
  assert.equal(FigurePixelGuideStrokeWidth, 1);
});

// --- Widget smoke test ----------------------------------------------------
//
// Loads every widget under scripts/docs/ in a single shared
// sandbox in dependency order and verifies none of them throws at
// load time. Catches syntax errors, missing globals, and broken
// cross-widget inheritance before they reach the rendered Doxygen
// page.
//
// All widgets now use ES6 class syntax. The `Class()` factory is
// preserved as a compatibility shim (covered by the unit tests
// above) but no production widget uses it any more — kept for the
// hypothetical contributor who copy-pastes the legacy pattern.
//
// Doxygen pages that use multiple widgets load them as separate
// `<script>` tags into the same global scope, so we mirror that
// here: one shared sandbox, base widgets loaded first.

test('All widgets load without throwing', () => {
  // Known cross-widget dependencies — base must load before
  // dependents. Add entries here when a new cross-widget reference
  // appears.
  const dependencyOrder = ['angle_from_x.js'];

  const docsDir = path.resolve(__dirname, '..', 'docs');
  const allWidgets = fs.readdirSync(docsDir)
    .filter(f => f.endsWith('.js'))
    .filter(f => f !== 'figure.js')
    .sort();

  // Move base-class widgets to the front. Order among the rest is
  // alphabetical for stable error messages.
  const widgets = [
    ...dependencyOrder,
    ...allWidgets.filter(w => !dependencyOrder.includes(w))
  ];

  assert.ok(widgets.length >= 18,
    `expected ≥18 widgets, found ${widgets.length}`);

  const sandbox = loadFigure();
  for (const widget of widgets) {
    const body = sandbox.document.createElement('body');
    const script = sandbox.document.createElement('script');
    body.appendChild(script);
    // Both anchor patterns supported: the modernised widgets use
    // `document.currentScript`; the `Class()` shim still routes
    // through `document.scripts[length-1]`. Set both so either
    // works in the smoke-test sandbox.
    sandbox.document.currentScript = script;
    sandbox.document.scripts = [script];

    const source = fs.readFileSync(path.join(docsDir, widget), 'utf8');
    assert.doesNotThrow(
      () => vm.runInContext(source, sandbox),
      `widget ${widget} threw during load`
    );
  }
});

test('Rasterizer perspective UV widget emits UV grid lines', () => {
  const body = loadWidget('rasterizer_perspective_uv.js');
  assert.equal(countElements(body, 'input'), 1,
    'perspective depth should remain a scalar slider control');
  assert.equal(countElements(body, 'path'), 32,
    'perspective UV widget should draw stroked UV grid lines, not just the quad outline');
  assert.equal(countElements(body, 'polygon'), 4,
    'each panel should draw a filled quad and a stroked quad outline');
});

test('Texture coordinate mapping widget exposes mapping controls and lookup state', () => {
  const body = loadWidget('texture_coordinate_mapping.js');
  assert.equal(countElements(body, 'button'), 2,
    'mapping mode should be a planar/UV segmented control');
  assert.equal(countElements(body, 'input'), 2,
    'U and V scale should use scalar sliders');

  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'sample-point');
  assert.equal(handles.length, 1,
    'sample point should be manipulated through a visible draggable handle');

  const previewCells = elementsByTag(body, 'rect')
    .filter(r => r.attributes['data-preview-cell']);
  assert.equal(previewCells.length, 36,
    'sampled texture preview should draw a small checker grid');

  const readouts = elementsByTag(body, 'text')
    .filter(t => t.attributes['data-readout']);
  assert.deepEqual(readouts.map(t => t.attributes['data-readout']),
    ['texture-coordinates', 'checker-parity']);
  assert.ok(textContents(body).join(' ').includes('floor(s) + floor(t)'),
    'widget should show the checker parity formula');
});

test('Camera forward projection widget exposes projection controls', () => {
  const body = loadWidget('camera_forward_projection.js');
  assert.equal(countElements(body, 'button'), 2,
    'camera projection widget should toggle pinhole and orthographic modes');
  assert.equal(countElements(body, 'input'), 0,
    'world point position should use a draggable handle instead of scalar sliders');

  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'world-point');
  assert.equal(handles.length, 1,
    'widget should expose one visible draggable world point');

  const projectedPixels = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-projected-pixel'] === 'true');
  assert.equal(projectedPixels.length, 1,
    'widget should mark the projected pixel on the view plane');

  const text = textContents(body).join(' ');
  assert.ok(text.includes('clip space'), 'widget should label the clip-space readout');
  assert.ok(text.includes('w ='), 'widget should show homogeneous w');
});

test('Rasterizer clipping widget omits coordinate labels', () => {
  const body = loadWidget('rasterizer_clip_attributes.js');
  const text = textContents(body).join(' ');
  assert.ok(!/\bnew\b/.test(text), 'generated clip vertices should not be labeled "new"');
  assert.ok(!/\bp\d\b/.test(text), 'source vertices should not show p0/p1/p2 coordinate labels');
  assert.equal(countElements(body, 'input'), 0,
    'spatial source vertex controls should not use scalar sliders');
  assert.ok(countElements(body, 'rect') >= 4,
    'widget should still draw viewport and generated clip-vertex markers');
  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'source-vertex');
  assert.equal(handles.length, 3,
    'all source vertices should be manipulated through visible draggable handles');
  assert.deepEqual(handles.map(c => c.attributes['data-vertex-index']), ['0', '1', '2']);
});

test('Rasterizer MSAA widget emits samples and partial resolves', () => {
  const body = loadWidget('rasterizer_msaa_coverage.js');
  const text = textContents(body).join(' ');
  assert.ok(text.includes('4x MSAA'),
    'MSAA widget should default to a multi-sample resolve view');
  assert.equal(countElements(body, 'button'), 4,
    'widget should expose the supported 1x/2x/4x/8x sample counts');
  assert.equal(countElements(body, 'input'), 0,
    'spatial triangle control should use draggable vertices, not a scalar slider');
  assert.ok(countElements(body, 'circle') >= 9 * 6 * 4,
    'default 4x mode should draw one visible dot per subpixel sample');

  const pixelRects = elementsByTag(body, 'rect')
    .filter(r => r.attributes['data-sample-count'] === '4');
  assert.equal(pixelRects.length, 9 * 6,
    'widget should emit one resolvable rectangle per pixel');
  assert.ok(pixelRects.some(r => ['1', '2', '3'].includes(r.attributes['data-covered-samples'])),
    'at least one edge pixel should resolve to a fractional 4x coverage value');

  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'triangle-vertex');
  assert.equal(handles.length, 3,
    'triangle geometry should be manipulated through visible draggable vertices');
});

test('Motion blur widget exposes shutter-time sampling controls', () => {
  const body = loadWidget('motion_blur_time_sampling.js');
  const text = textContents(body).join(' ');
  assert.ok(text.includes('current t = 0.50'),
    'motion blur widget should default to the half-shutter position');
  assert.ok(text.includes('sampled positions'),
    'widget should label the sampled position marks');
  assert.equal(countElements(body, 'input'), 1,
    'shutter time should be exposed as a scalar slider');
  assert.equal(countElements(body, 'button'), 2,
    'regular vs stochastic sampling should be a segmented control');

  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'velocity-end');
  assert.equal(handles.length, 1,
    'velocity should be manipulated through a visible endpoint handle');

  const samples = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-time-sample'] === '1');
  assert.equal(samples.length, 8,
    'default stochastic mode should draw every shutter-time sample');
  assert.ok(new Set(samples.map(c => c.attributes['data-sample-time'])).size > 1,
    'stochastic mode should distribute samples across shutter time');

  const ghosts = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-ghost-silhouette'] === '1');
  assert.equal(ghosts.length, 8,
    'accumulated silhouettes should mirror the time samples');
});

test('ViewPlane iteration widget exposes mode and progress controls', () => {
  const body = loadWidget('viewplane_iteration_order.js');
  assert.equal(countElements(body, 'button'), 6,
    'viewplane widget should expose every documented iteration mode');
  assert.equal(countElements(body, 'input'), 1,
    'viewplane widget should expose render progress as a scalar slider');
  assert.ok(textContents(body).join(' ').includes('Point interlaced'),
    'default mode should name the active iteration strategy');

  const cells = elementsByTag(body, 'rect')
    .filter(r => r.attributes['data-viewplane-cell']);
  assert.equal(cells.length, 16 * 10,
    'widget should draw one visible cell for every pixel in the image grid');
  assert.ok(cells.some(r => r.attributes['data-rendered'] === '1'),
    'default progress should show already-rendered pixels');
  assert.ok(cells.every(r => r.attributes['data-rendered'] === '1'),
    'point interlacing should give early whole-frame coverage');
  assert.ok(new Set(cells.map(r => r.attributes.fill)).size > 1,
    'cell colors should preserve the visible traversal order');
});

test('Rasterizer pipeline widget uses draggable vertex handles', () => {
  const body = loadWidget('rasterizer_pipeline.js');
  assert.equal(countElements(body, 'button'), 2,
    'pipeline widget should expose the barycentric/UV color mode control');
  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'triangle-vertex');
  assert.equal(handles.length, 3,
    'pipeline triangle should keep its three visible draggable vertices');
});

test('Sampler streams widget exposes sampler and dimension controls', () => {
  const body = loadWidget('sampler_streams.js');
  const text = textContents(body).join(' ');

  assert.equal(countElements(body, 'button'), 9,
    'sampler widget should expose sampler type, sample count, and dimension mode controls');
  assert.equal(countElements(body, 'input'), 0,
    'sampler widget should use segmented controls for discrete choices');
  assert.ok(text.includes('pixel jitter'), 'widget should label the pixel sample dimension');
  assert.ok(text.includes('lens sample'), 'widget should label the lens sample dimension');
  assert.ok(text.includes('shutter time'), 'widget should label the shutter-time sample dimension');
  assert.ok(text.includes('different pre-baked set'),
    'default view should explain independent stream dimensions');

  const panels = elementsByTag(body, 'rect')
    .filter(r => r.attributes['data-sample-panel']);
  assert.deepEqual(panels.map(r => r.attributes['data-sample-panel']),
    ['pixel', 'pixel', 'lens', 'shutter-time']);

  const sampleDots = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-sample-dot']);
  assert.equal(sampleDots.length, 16 * 4,
    'default 16-sample view should draw every sample in each panel');

  const lensDots = sampleDots.filter(c => c.attributes['data-sample-dot'] === 'lens');
  const shutterDots = sampleDots.filter(c => c.attributes['data-sample-dot'] === 'shutter-time');
  assert.equal(lensDots.length, 16);
  assert.equal(shutterDots.length, 16);
});

test('Grid DDA widget exposes traversal state and controls', () => {
  const body = loadWidget('grid_dda_traversal.js');
  assert.equal(countElements(body, 'input'), 1,
    'grid density should be adjusted with one scalar slider');

  const text = textContents(body).join(' ');
  assert.ok(text.includes('grid density'),
    'density control should be labeled');
  assert.ok(text.includes('t_next x'),
    'widget should expose the next x-boundary parameter');
  assert.ok(text.includes('t_next y'),
    'widget should expose the next y-boundary parameter');
  assert.ok(text.includes('visited cells'),
    'widget should summarize the traversal trail');

  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle']);
  assert.deepEqual(handles.map(c => c.attributes['data-drag-handle']),
    ['ray-origin', 'ray-direction']);

  const visitedCells = elementsByTag(body, 'rect')
    .filter(r => r.attributes['data-grid-dda-cell'] === 'visited');
  assert.ok(visitedCells.length >= 4,
    'default ray should visit multiple cells');
  assert.equal(elementsByTag(body, 'rect')
    .filter(r => r.attributes['data-grid-dda-cell'] === 'current').length, 1,
    'the current cell should be highlighted separately');
  assert.equal(elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-grid-dda-entry'] === '1').length, 1,
    'the ray entry point should be marked');
});

test('Mesh triangle interpolation widget exposes hit and normal controls', () => {
  const body = loadWidget('mesh_triangle_interpolation.js');
  const text = textContents(body).join(' ');
  assert.ok(text.includes('alpha'), 'widget should show barycentric alpha');
  assert.ok(text.includes('beta'), 'widget should show barycentric beta');
  assert.ok(text.includes('gamma'), 'widget should show barycentric gamma');
  assert.ok(text.includes('UV uses the same weights'),
    'widget should connect barycentric weights to UV interpolation');
  assert.equal(countElements(body, 'input'), 0,
    'spatial triangle and hit-point controls should use direct manipulation');
  assert.equal(countElements(body, 'button'), 2,
    'normal interpolation should be toggled with flat/smooth segmented buttons');

  const vertexHandles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'triangle-vertex');
  assert.equal(vertexHandles.length, 3,
    'all triangle vertices should have visible draggable handles');
  assert.deepEqual(vertexHandles.map(c => c.attributes['data-vertex-index']), ['0', '1', '2']);

  const hitHandles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'triangle-hit-point');
  assert.equal(hitHandles.length, 1,
    'the interpolated hit point should have one visible draggable handle');

  const normalVectors = elementsByTag(body, 'line')
    .filter(line => line.attributes['data-normal-vector']);
  assert.ok(normalVectors.some(line => line.attributes['data-normal-vector'] === 'smooth'),
    'default mode should draw the smooth interpolated normal');
  assert.equal(elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-interpolated-uv'] === '1').length, 1,
    'widget should draw the interpolated UV sample');
});

test('Farthest-point widgets use explicit angle sliders', () => {
  [
    'box_farthest_point.js',
    'convex_hull_farthest_point.js',
    'sphere_farthest_point.js',
  ].forEach((widget) => {
    const body = loadWidget(widget);
    assert.equal(countElements(body, 'input'), 1,
      `${widget} should expose its direction as a scalar slider`);
    assert.ok(textContents(body).join(' ').includes('direction angle'),
      `${widget} should label the angle control`);
    const resultPoints = elementsByTag(body, 'circle')
      .filter(c => c.attributes.class === 'result');
    assert.ok(resultPoints.some(c => Number(c.attributes.r) >= 0.14),
      `${widget} should make the farthest result point prominent`);
  });
});

test('Angle widgets use the shared scalar angle slider', () => {
  [
    'angle_from_clock.js',
    'angle_from_degrees.js',
    'angle_from_radians.js',
    'angle_from_turns.js',
  ].forEach((widget) => {
    const body = loadWidgets(['angle_from_x.js', widget]);
    const inputs = elementsByTag(body, 'input');
    assert.equal(inputs.length, 1,
      `${widget} should expose its angle as a scalar slider`);
    assert.equal(String(inputs[0].max), '720',
      `${widget} should show the non-bijective second revolution`);
    assert.ok(textContents(body).join(' ').includes('angle'),
      `${widget} should label the angle control`);
  });
});

test('Ray-at widget uses an explicit t slider', () => {
  const body = loadWidget('ray_at.js');
  assert.equal(countElements(body, 'input'), 1,
    'ray_at.js should expose ray parameter t as a scalar slider');
  assert.ok(textContents(body).join(' ').includes('t'),
    'ray_at.js should label the t control');
  const resultPoints = elementsByTag(body, 'circle')
    .filter(c => c.attributes.class === 'result');
  assert.ok(resultPoints.some(c => Number(c.attributes.r) >= 0.14),
    'ray_at.js should make the evaluated point prominent');
});

test('Ray-project widget exposes draggable source points', () => {
  const body = loadWidget('ray_project.js');
  assert.equal(countElements(body, 'input'), 0,
    'ray_project.js should use direct manipulation for spatial points');
  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'project-point');
  assert.equal(handles.length, 8,
    'ray_project.js should expose every generated point as a visible drag handle');
  assert.deepEqual(handles.map(c => c.attributes['data-point-index']),
    ['0', '1', '2', '3', '4', '5', '6', '7']);
});

test('Wide-angle camera mapping widget exposes camera modes and image handle', () => {
  const body = loadWidget('wide_angle_camera_mappings.js');
  const text = textContents(body).join(' ');
  assert.ok(text.includes('fisheye'), 'widget should expose the fisheye mode');
  assert.ok(text.includes('spherical'), 'widget should expose the spherical mode');
  assert.ok(text.includes('equirectangular'), 'widget should expose the equirectangular mode');
  assert.ok(text.includes('fieldOfView'), 'fisheye mode should expose an FOV slider');
  assert.ok(text.includes('unit sphere direction'), 'widget should label the sphere-side mapping');

  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle'] === 'image-point');
  assert.equal(handles.length, 1,
    'image coordinate should be manipulated through one visible draggable handle');
  assert.equal(countElements(body, 'button'), 3,
    'widget should use a segmented mode control for the three camera mappings');
  assert.equal(countElements(body, 'input'), 3,
    'widget should keep the three FOV controls available for mode switching');
  assert.equal(elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-fisheye-valid-disc'] === '1').length, 1,
    'fisheye mode should draw the valid unit-disc cutoff');
  assert.equal(elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-ray-direction-marker'] === '1').length, 1,
    'widget should mark the mapped ray direction on the sphere');

  const buttons = elementsByTag(body, 'button');
  buttons.find(button => button.textContent === 'spherical').click();
  assert.equal(elementsByTag(body, 'rect')
    .filter(r => r.attributes['data-partial-panorama'] === '1').length, 1,
    'spherical mode should draw the partial-panorama image window');
  assert.ok(textContents(body).join(' ').includes('horizontalFieldOfView'),
    'spherical mode should expose the horizontal FOV slider');
  assert.ok(textContents(body).join(' ').includes('verticalFieldOfView'),
    'spherical mode should expose the vertical FOV slider');

  buttons.find(button => button.textContent === 'equirectangular').click();
  assert.equal(elementsByTag(body, 'line')
    .filter(line => line.attributes['data-equirectangular-seam'] === '1').length, 2,
    'equirectangular mode should mark both horizontal seam edges');
  assert.equal(elementsByTag(body, 'rect')
    .filter(rect => rect.attributes['data-pole-stretch']).length, 2,
    'equirectangular mode should mark both pole-stretched image rows');
});

test('Instance transform widget exposes ray handles and transform modes', () => {
  const body = loadWidget('instance_transform_normals.js');
  assert.equal(countElements(body, 'input'), 2,
    'scale and rotation should be scalar sliders');
  assert.equal(countElements(body, 'button'), 3,
    'point, direction, and normal views should be segmented controls');

  const handles = elementsByTag(body, 'circle')
    .filter(c => c.attributes['data-drag-handle']);
  assert.deepEqual(handles.map(c => c.attributes['data-drag-handle']),
    ['ray-origin', 'ray-end'],
    'world ray should be manipulated through visible origin/end handles');

  const paths = elementsByTag(body, 'path')
    .filter(p => p.attributes.stroke === '#1864ab' || p.attributes.stroke === '#d9480f');
  assert.ok(paths.length >= 2,
    'widget should draw both transformed world object and local wrapped primitive');

  const text = textContents(body).join(' ');
  assert.ok(text.includes('world ray'));
  assert.ok(text.includes('local-space ray'));
  assert.ok(text.includes('geometric normal'));
  assert.ok(text.includes('inverse-transpose normal'));

  const normalLines = elementsByTag(body, 'line')
    .filter(line => [
      'geometric-normal',
      'inverse-transpose-normal',
      'naive-normal',
    ].includes(line.attributes['data-transform-view']));
  assert.equal(normalLines.length, 3,
    'normal view should compare geometric, inverse-transpose, and direction-scaled normals');
});

test('Bounding-box spatial widgets use visible drag handles', () => {
  [
    ['bounding_box_include.js', 'included-point'],
    ['bounding_box_moved_by.js', 'move-vector-end'],
    ['bounding_box_grown_by.js', 'growth-vector-end'],
  ].forEach(([widget, handleName]) => {
    const body = loadWidget(widget);
    assert.equal(countElements(body, 'input'), 0,
      `${widget} should not use scalar controls for spatial state`);
    const handles = elementsByTag(body, 'circle')
      .filter(c => c.attributes['data-drag-handle'] === handleName);
    assert.equal(handles.length, 1,
      `${widget} should expose a visible ${handleName} drag handle`);
    if (widget === 'bounding_box_grown_by.js') {
      assert.ok(Number(handles[0].attributes.cx) > 2,
        'grown-by drag handle should start at the top-right growth endpoint');
      assert.ok(Number(handles[0].attributes.cy) < -2,
        'grown-by drag handle should start above the original top-right corner');
    }
  });
});

test('Bounding-box boolean widgets expose draggable source boxes', () => {
  ['bounding_box_and.js', 'bounding_box_or.js'].forEach((widget) => {
    const body = loadWidget(widget);
    const sourceBoxes = elementsByTag(body, 'rect')
      .filter(r => r.attributes['data-drag-handle'] === 'source-box');
    assert.equal(sourceBoxes.length, 2,
      `${widget} should expose both source boxes as direct drag targets`);
    assert.deepEqual(sourceBoxes.map(r => r.attributes['data-box-index']), ['0', '1']);
  });
});

test('Production widgets no longer instantiate DragHandler', () => {
  const docsDir = path.resolve(__dirname, '..', 'docs');
  const offenders = fs.readdirSync(docsDir)
    .filter(file => file.endsWith('.js') && file !== 'figure.js')
    .filter(file => fs.readFileSync(path.join(docsDir, file), 'utf8').includes('new DragHandler'));
  assert.deepEqual(offenders, []);
});

test('Production widgets use shared stroke width constants', () => {
  const docsDir = path.resolve(__dirname, '..', 'docs');
  const offenders = fs.readdirSync(docsDir)
    .filter(file => file.endsWith('.js') && file !== 'figure.js')
    .filter((file) => {
      const source = fs.readFileSync(path.join(docsDir, file), 'utf8');
      return /['"]stroke-width['"]\s*:\s*[0-9.]+/.test(source);
    });
  assert.deepEqual(offenders, []);
});

test('Color model conversion widget shows RGB storage and helper views', () => {
  const body = loadWidget('color_model_conversions.js');
  const text = textContents(body).join(' ');
  assert.ok(text.includes('RGB storage'),
    'widget should identify RGB as the stored representation');
  assert.ok(text.includes('HSV helper view'),
    'widget should show HSV as a computed helper view');
  assert.ok(text.includes('CMYK helper view'),
    'widget should show CMYK as a computed helper view');
  assert.ok(text.includes('fromHSV()'));
  assert.ok(text.includes('fromCMYK()'));
  assert.ok(countElements(body, 'rect') >= 15,
    'widget should include swatches and component bars for each model');
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

// --- Figure v2 primitives --------------------------------------------------

test('FigureSvg: creates scoped SVGs and can clear children', () => {
  const { FigureSvg } = loadFigure();
  const canvas = new FigureSvg({ width: 100, height: 50 });
  assert.equal(canvas.element.tagName, 'svg');
  assert.equal(canvas.element.getAttribute('class'), 'figure-widget-svg');
  canvas.add('circle', { cx: 10, cy: 10, r: 3 });
  canvas.add('rect', { x: 0, y: 0, width: 5, height: 5 });
  assert.equal(canvas.element.children.length, 2);
  canvas.clear();
  assert.equal(canvas.element.children.length, 0);
});

test('FigureSvg: maps pointer events through the rendered SVG transform', () => {
  const { FigureSvg } = loadFigure();
  const canvas = new FigureSvg({ width: 100, height: 50 });
  canvas.element.createSVGPoint = () => ({
    x: 0,
    y: 0,
    matrixTransform(matrix) {
      return matrix.transform({ x: this.x, y: this.y });
    },
  });
  canvas.element.getScreenCTM = () => ({
    inverse() {
      return {
        transform(point) {
          return {
            x: (point.x - 20) / 2,
            y: (point.y - 30) / 3,
          };
        },
      };
    },
  });

  const point = canvas.pointFromEvent({ clientX: 220, clientY: 180 });
  assert.equal(point.x, 100);
  assert.equal(point.y, 50);
});

test('FigureSvg: accounts for preserveAspectRatio letterboxing', () => {
  const { FigureSvg } = loadFigure();
  const canvas = new FigureSvg({ width: 384, height: 256 });
  canvas.element.getBoundingClientRect = () => ({
    left: 20,
    top: 40,
    width: 600,
    height: 360,
  });

  const topLeft = canvas.pointFromEvent({ clientX: 50, clientY: 40 });
  assert.equal(topLeft.x, 0);
  assert.equal(topLeft.y, 0);

  const bottomRight = canvas.pointFromEvent({ clientX: 590, clientY: 400 });
  assert.equal(bottomRight.x, 384);
  assert.equal(bottomRight.y, 256);
});

test('FigureSegmentedControl: exposes options and active state', () => {
  const { FigureSegmentedControl } = loadFigure();
  const control = new FigureSegmentedControl({
    label: 'samples',
    value: 4,
    options: [1, 2, 4, 8].map(n => ({ label: `${n}x`, value: n })),
  });
  const element = control.element();
  assert.equal(countElements(element, 'button'), 4);
  assert.ok(textContents(element).join(' ').includes('samples'));
  assert.equal(element.children[3].className, 'is-active');
  control.update(8);
  assert.equal(element.children[4].className, 'is-active');
});

test('FigureSliderControl: exposes range input and formatted value', () => {
  const { FigureSliderControl } = loadFigure();
  const control = new FigureSliderControl({
    label: 'depth',
    min: 1,
    max: 6,
    step: 0.5,
    value: 3,
    precision: 1,
    format: value => value.toFixed(1).replace(/\.0$/, ''),
  });
  const element = control.element();
  assert.equal(countElements(element, 'input'), 1);
  assert.equal(element.children[1].type, 'range');
  assert.equal(element.children[2].textContent, '3');
  control.update(3.5);
  assert.equal(element.children[2].textContent, '3.5');
});
