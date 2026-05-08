// Interactive widget for homogeneous/screen clipping preserving
// interpolated attributes. The point outside the viewport can move;
// each generated clip vertex carries linearly interpolated UVs.

(function(scriptElement) {
  const svgns = 'http://www.w3.org/2000/svg';
  const W = 560;
  const H = 300;
  const clip = { left: 145, top: 54, right: 430, bottom: 246 };

  function svg(name, attrs) {
    const e = document.createElementNS(svgns, name);
    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    }
    return e;
  }

  function uvColor(v) {
    return `rgb(${Math.round(255 * v.u)}, ${Math.round(255 * v.v)}, 0)`;
  }

  function interpolate(a, b, t, generated) {
    return {
      x: a.x + (b.x - a.x) * t,
      y: a.y + (b.y - a.y) * t,
      u: a.u + (b.u - a.u) * t,
      v: a.v + (b.v - a.v) * t,
      generated,
    };
  }

  function clipAgainst(poly, inside, intersect) {
    if (poly.length === 0) return [];
    const out = [];
    let prev = poly[poly.length - 1];
    let prevInside = inside(prev);
    for (const curr of poly) {
      const currInside = inside(curr);
      if (currInside !== prevInside) {
        out.push(intersect(prev, curr));
      }
      if (currInside) out.push(curr);
      prev = curr;
      prevInside = currInside;
    }
    return out;
  }

  function clipPolygon(poly) {
    let p = poly;
    p = clipAgainst(
      p,
      v => v.x >= clip.left,
      (a, b) => interpolate(a, b, (clip.left - a.x) / (b.x - a.x), true));
    p = clipAgainst(
      p,
      v => v.x <= clip.right,
      (a, b) => interpolate(a, b, (clip.right - a.x) / (b.x - a.x), true));
    p = clipAgainst(
      p,
      v => v.y >= clip.top,
      (a, b) => interpolate(a, b, (clip.top - a.y) / (b.y - a.y), true));
    p = clipAgainst(
      p,
      v => v.y <= clip.bottom,
      (a, b) => interpolate(a, b, (clip.bottom - a.y) / (b.y - a.y), true));
    return p;
  }

  function build(outsideX) {
    const original = [
      { x: outsideX, y: 150, u: 0.00, v: 0.50, generated: false },
      { x: 360,      y: 42,  u: 1.00, v: 0.00, generated: false },
      { x: 384,      y: 262, u: 1.00, v: 1.00, generated: false },
    ];
    const clipped = clipPolygon(original);

    const root = svg('svg', {
      width: W,
      height: H,
      viewBox: `0 0 ${W} ${H}`,
      style: 'background: #fafafa; user-select: none;',
    });

    root.appendChild(svg('rect', {
      x: clip.left,
      y: clip.top,
      width: clip.right - clip.left,
      height: clip.bottom - clip.top,
      fill: '#fff',
      stroke: '#333',
      'stroke-width': 2,
    }));

    root.appendChild(svg('polygon', {
      points: original.map(v => `${v.x},${v.y}`).join(' '),
      fill: 'none',
      stroke: '#999',
      'stroke-width': 2,
      'stroke-dasharray': '7 5',
    }));

    if (clipped.length >= 3) {
      root.appendChild(svg('polygon', {
        points: clipped.map(v => `${v.x},${v.y}`).join(' '),
        fill: '#83c5be',
        'fill-opacity': 0.25,
        stroke: '#0b7285',
        'stroke-width': 3,
      }));
    }

    original.forEach((v) => {
      root.appendChild(svg('circle', {
        cx: v.x, cy: v.y, r: 7,
        fill: uvColor(v),
        stroke: '#111',
        'stroke-width': 1.5,
      }));
    });

    clipped.forEach((v) => {
      if (!v.generated) return;
      root.appendChild(svg('rect', {
        x: v.x - 6, y: v.y - 6,
        width: 12, height: 12,
        fill: uvColor(v),
        stroke: '#111',
        'stroke-width': 1.5,
      }));
    });

    root.appendChild(svg('text', {
      x: clip.left + 8, y: clip.top + 18,
      'font-family': 'sans-serif',
      'font-size': 13,
      fill: '#333',
    })).textContent = 'viewport clip rectangle';

    return root;
  }

  const container = document.createElement('div');
  const controls = document.createElement('div');
  controls.style.margin = '0 0 8px 0';
  controls.style.display = 'flex';
  controls.style.alignItems = 'center';
  controls.style.gap = '8px';
  const label = document.createElement('label');
  label.textContent = 'outside vertex x';
  const slider = document.createElement('input');
  slider.type = 'range';
  slider.min = '20';
  slider.max = '170';
  slider.step = '1';
  slider.value = '42';
  const value = document.createElement('span');
  value.textContent = slider.value;
  controls.appendChild(label);
  controls.appendChild(slider);
  controls.appendChild(value);
  container.appendChild(controls);

  let svgEl = build(parseFloat(slider.value));
  container.appendChild(svgEl);
  slider.addEventListener('input', () => {
    value.textContent = slider.value;
    const next = build(parseFloat(slider.value));
    container.replaceChild(next, svgEl);
    svgEl = next;
  });

  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
