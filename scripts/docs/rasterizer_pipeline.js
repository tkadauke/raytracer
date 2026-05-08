// Interactive widget for `engine::raster::Rasterizer`'s
// edge-function rasterization (Pineda 1988). Shows a triangle,
// its axis-aligned bounding box (the rasterizer's scan region),
// and the per-pixel inside-test live as you mouse around.
//
// Teaching points the static images can't show:
//
//  - The "inside" decision is three signed sub-areas (w0, w1, w2).
//    A pixel is inside iff all three signs match the parent
//    triangle's signed area.
//  - The barycentric weights (w0, w1, w2) at any pixel are exactly
//    those sub-areas divided by the parent area, summing to 1.0.
//    Drag the cursor across the triangle to see them slide.
//  - Outside the triangle, at least one weight goes negative.
//    Click to place the cursor and freeze the readout.
//  - The bounding box is what the rasterizer actually scans.
//    Pixels in the box but outside the triangle are tested and
//    rejected — there's no clever tight-fit optimisation in V1.
//
// Drag the three vertex handles to reshape the triangle and watch
// the rasterized region update in real time.

(function(scriptElement) {
  // Vertex positions in pixel-grid coords (cells, not SVG pixels).
  const initialVerts = [
    { x: 1.5, y: 4.5 },
    { x: 7.5, y: 5.0 },
    { x: 4.0, y: 1.0 },
  ];

  // Edge function — twice the signed sub-area of triangle (a, b, p).
  function edge(ax, ay, bx, by, px, py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
  }

  // SVG namespace; Canvas + figure helpers from figure.js handle
  // the chrome but for the live mouse-tracking we drop to raw SVG.
  const svgns = 'http://www.w3.org/2000/svg';

  // Render-grid resolution. The canvas is 12 wide by 8 tall in
  // these "cells"; each cell renders as a 32×32 SVG pixel block,
  // giving a 384×256 viewport that's readable at thumbnail sizes.
  const COLS = 12;
  const ROWS = 8;
  const CELL = 32;
  const vertexUVs = [
    { u: 0.0, v: 1.0 },
    { u: 1.0, v: 1.0 },
    { u: 0.5, v: 0.0 },
  ];

  function svg(name, attrs) {
    const e = document.createElementNS(svgns, name);
    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) {
        e.setAttribute(k, v);
      }
    }
    return e;
  }

  function interpolatedUV(b0, b1, b2) {
    return {
      u: vertexUVs[0].u * b0 + vertexUVs[1].u * b1 + vertexUVs[2].u * b2,
      v: vertexUVs[0].v * b0 + vertexUVs[1].v * b1 + vertexUVs[2].v * b2,
    };
  }

  function fillForWeights(b0, b1, b2, mode) {
    if (mode === 'uv') {
      const uv = interpolatedUV(b0, b1, b2);
      return `rgb(${Math.round(255 * uv.u)}, ${Math.round(255 * uv.v)}, 0)`;
    }

    // R-G-B per vertex: vertex 0 = red, vertex 1 = green,
    // vertex 2 = blue. Interpolated weights produce a smooth
    // colour gradient across the triangle.
    return `rgb(${Math.round(255 * b0)}, ${Math.round(255 * b1)}, ${Math.round(255 * b2)})`;
  }

  function build(verts, cursor, mode) {
    const root = svg('svg', {
      width: COLS * CELL,
      height: ROWS * CELL,
      viewBox: `0 0 ${COLS * CELL} ${ROWS * CELL}`,
      style: 'background: #fafafa; user-select: none; cursor: crosshair;',
    });

    const [p0, p1, p2] = verts;
    // Twice the signed area of the parent triangle. Sign tells us
    // winding; magnitude is used to normalise the barycentric
    // weights below.
    const area = edge(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);

    // Bounding box (clamped to the canvas grid).
    const minX = Math.max(0, Math.floor(Math.min(p0.x, p1.x, p2.x)));
    const maxX = Math.min(COLS - 1, Math.ceil(Math.max(p0.x, p1.x, p2.x)));
    const minY = Math.max(0, Math.floor(Math.min(p0.y, p1.y, p2.y)));
    const maxY = Math.min(ROWS - 1, Math.ceil(Math.max(p0.y, p1.y, p2.y)));

    // Pixel grid background — light grey lines at every cell boundary
    // so the rasterizer's discretisation is visible.
    for (let y = 0; y <= ROWS; y++) {
      root.appendChild(svg('line', {
        x1: 0, y1: y * CELL, x2: COLS * CELL, y2: y * CELL,
        stroke: '#ddd', 'stroke-width': 1,
      }));
    }
    for (let x = 0; x <= COLS; x++) {
      root.appendChild(svg('line', {
        x1: x * CELL, y1: 0, x2: x * CELL, y2: ROWS * CELL,
        stroke: '#ddd', 'stroke-width': 1,
      }));
    }

    // Bounding-box highlight — the region the rasterizer actually
    // scans. Pixels inside the box but outside the triangle still
    // get tested, just rejected.
    root.appendChild(svg('rect', {
      x: minX * CELL, y: minY * CELL,
      width: (maxX - minX + 1) * CELL, height: (maxY - minY + 1) * CELL,
      fill: 'none', stroke: '#888', 'stroke-width': 2,
      'stroke-dasharray': '4 4',
    }));

    // Filled pixels — every cell in the bounding box that passes
    // the inside test, coloured by interpolated barycentric weights
    // (each vertex contributes its own R/G/B tint).
    if (Math.abs(area) > 0) {
      for (let py = minY; py <= maxY; py++) {
        for (let px = minX; px <= maxX; px++) {
          const cx = px + 0.5;
          const cy = py + 0.5;
          const w0 = edge(p1.x, p1.y, p2.x, p2.y, cx, cy);
          const w1 = edge(p2.x, p2.y, p0.x, p0.y, cx, cy);
          const w2 = area - w0 - w1;
          const inside = (area > 0)
            ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
            : (w0 <= 0 && w1 <= 0 && w2 <= 0);
          if (!inside) continue;
          const b0 = w0 / area;
          const b1 = w1 / area;
          const b2 = w2 / area;
          root.appendChild(svg('rect', {
            x: px * CELL, y: py * CELL,
            width: CELL, height: CELL,
            fill: fillForWeights(b0, b1, b2, mode),
            'fill-opacity': 0.85,
          }));
        }
      }
    }

    // Triangle edges, drawn over the filled pixels.
    root.appendChild(svg('polygon', {
      points: verts.map(v => `${v.x * CELL},${v.y * CELL}`).join(' '),
      fill: 'none', stroke: '#222', 'stroke-width': 2.5,
    }));

    // Vertex handles — drag-to-move circles labelled p0/p1/p2.
    const labels = ['p0', 'p1', 'p2'];
    const colours = ['#d22', '#2c2', '#26d'];
    verts.forEach((v, i) => {
      root.appendChild(svg('circle', {
        cx: v.x * CELL, cy: v.y * CELL, r: 9,
        fill: colours[i], stroke: '#000', 'stroke-width': 2,
        'data-vertex-index': i,
        style: 'cursor: grab;',
      }));
      root.appendChild(svg('text', {
        x: v.x * CELL + 14, y: v.y * CELL - 10,
        'font-size': 13, 'font-family': 'monospace',
        fill: colours[i], 'font-weight': 'bold',
      })).textContent = labels[i];
    });

    // Cursor marker + barycentric readout.
    if (cursor && Math.abs(area) > 0) {
      const w0 = edge(p1.x, p1.y, p2.x, p2.y, cursor.x, cursor.y);
      const w1 = edge(p2.x, p2.y, p0.x, p0.y, cursor.x, cursor.y);
      const w2 = area - w0 - w1;
      const b0 = w0 / area;
      const b1 = w1 / area;
      const b2 = w2 / area;
      const uv = interpolatedUV(b0, b1, b2);
      const inside = (area > 0)
        ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
        : (w0 <= 0 && w1 <= 0 && w2 <= 0);
      root.appendChild(svg('circle', {
        cx: cursor.x * CELL, cy: cursor.y * CELL, r: 5,
        fill: inside ? '#fff' : '#fff8',
        stroke: '#000', 'stroke-width': 2,
        'pointer-events': 'none',
      }));
      const readoutBg = svg('rect', {
        x: 6, y: ROWS * CELL - 64, width: 230, height: 58,
        fill: '#fffe', stroke: '#888', 'stroke-width': 1, rx: 4,
        'pointer-events': 'none',
      });
      root.appendChild(readoutBg);
      const fmt = n => (n >= 0 ? ' ' : '') + n.toFixed(2);
      const lines = [
        `w0 = ${fmt(b0)}   w1 = ${fmt(b1)}   w2 = ${fmt(b2)}`,
        `uv = (${uv.u.toFixed(2)}, ${uv.v.toFixed(2)})  sum = ${fmt(b0 + b1 + b2)}`,
        `${inside ? 'inside' : 'outside'} the triangle`,
      ];
      lines.forEach((line, i) => {
        const t = svg('text', {
          x: 14, y: ROWS * CELL - 46 + i * 16,
          'font-size': 12, 'font-family': 'monospace',
          fill: '#222', 'pointer-events': 'none',
        });
        t.textContent = line;
        root.appendChild(t);
      });
    }

    return root;
  }

  const verts = initialVerts.map(v => ({ x: v.x, y: v.y }));
  let cursor = { x: 4.0, y: 3.5 };
  let mode = 'barycentric';
  const container = document.createElement('div');

  const controls = document.createElement('div');
  controls.style.margin = '0 0 8px 0';
  controls.style.display = 'flex';
  controls.style.gap = '8px';
  const baryButton = document.createElement('button');
  baryButton.textContent = 'Barycentric colour';
  const uvButton = document.createElement('button');
  uvButton.textContent = 'UV colour';
  controls.appendChild(baryButton);
  controls.appendChild(uvButton);
  container.appendChild(controls);

  function syncButtons() {
    baryButton.disabled = mode === 'barycentric';
    uvButton.disabled = mode === 'uv';
  }

  let svgEl = build(verts, cursor, mode);
  container.appendChild(svgEl);

  // Drag a vertex (mousedown on a circle with data-vertex-index)
  // or just hover anywhere to move the cursor readout.
  let dragging = null;
  function pixelToCell(ev) {
    const rect = svgEl.getBoundingClientRect();
    return {
      x: (ev.clientX - rect.left) / CELL,
      y: (ev.clientY - rect.top) / CELL,
    };
  }
  function rebuild() {
    const next = build(verts, cursor, mode);
    container.replaceChild(next, svgEl);
    svgEl = next;
    attach();
  }
  baryButton.addEventListener('click', () => {
    mode = 'barycentric';
    syncButtons();
    rebuild();
  });
  uvButton.addEventListener('click', () => {
    mode = 'uv';
    syncButtons();
    rebuild();
  });
  function attach() {
    svgEl.addEventListener('mousedown', (ev) => {
      const idx = ev.target.getAttribute && ev.target.getAttribute('data-vertex-index');
      if (idx !== null && idx !== undefined) {
        dragging = parseInt(idx, 10);
        ev.preventDefault();
      }
    });
    svgEl.addEventListener('mousemove', (ev) => {
      const cell = pixelToCell(ev);
      if (dragging !== null) {
        verts[dragging].x = Math.max(0, Math.min(COLS, cell.x));
        verts[dragging].y = Math.max(0, Math.min(ROWS, cell.y));
        rebuild();
      } else {
        cursor = { x: cell.x, y: cell.y };
        rebuild();
      }
    });
    svgEl.addEventListener('mouseup', () => { dragging = null; });
    svgEl.addEventListener('mouseleave', () => { dragging = null; });
  }
  syncButtons();
  attach();

  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
