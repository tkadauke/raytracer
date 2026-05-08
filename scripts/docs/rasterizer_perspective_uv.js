// Interactive widget for perspective-correct UV interpolation.
//
// The same projected planar quad is drawn twice. The left panel
// linearly spaces UV grid lines in screen space; the right panel
// projects those same UV grid lines from the 3D quad, matching the
// rasterizer's 1/z correction:
//
//   uv_pixel = (Σ_i w_i · uv_i / z_i) / (Σ_i w_i / z_i)
//
// Moving the depth slider pushes the right edge away from the camera.
// Affine interpolation keeps UVs evenly spaced on the projected quad,
// while the perspective-correct version compresses texture space with
// depth.

(function(scriptElement) {
  const svgns = 'http://www.w3.org/2000/svg';
  const panelW = 280;
  const panelH = 220;
  const gap = 24;
  const gridSteps = [0.2, 0.4, 0.6, 0.8];
  const samplesPerLine = 28;

  function svg(name, attrs) {
    const e = document.createElementNS(svgns, name);
    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    }
    return e;
  }

  function lerp(a, b, t) {
    return a + (b - a) * t;
  }

  function interpolatePoint(a, b, t) {
    return {
      x: lerp(a.x, b.x, t),
      y: lerp(a.y, b.y, t),
    };
  }

  function makeProjection(rightDepth) {
    const halfW = 1.0;
    const halfH = 0.62;
    const leftDepth = 1.0;
    const vertices3 = [
      { x: -halfW, y:  halfH, z: leftDepth },
      { x:  halfW, y:  halfH, z: rightDepth },
      { x:  halfW, y: -halfH, z: rightDepth },
      { x: -halfW, y: -halfH, z: leftDepth },
    ];
    const projected = vertices3.map(p => ({ x: p.x / p.z, y: p.y / p.z }));
    const xs = projected.map(p => p.x);
    const ys = projected.map(p => p.y);
    const minX = Math.min(...xs);
    const maxX = Math.max(...xs);
    const minY = Math.min(...ys);
    const maxY = Math.max(...ys);
    const scale = Math.min(190 / (maxX - minX), 130 / (maxY - minY));
    const midX = (minX + maxX) * 0.5;
    const midY = (minY + maxY) * 0.5;

    function toPanel(p) {
      return {
        x: panelW * 0.5 + (p.x - midX) * scale,
        y: 116 + (p.y - midY) * scale,
      };
    }

    const verts = projected.map(toPanel);
    return {
      verts,
      affinePoint(u, v) {
        const top = interpolatePoint(verts[3], verts[2], u);
        const bottom = interpolatePoint(verts[0], verts[1], u);
        return interpolatePoint(top, bottom, v);
      },
      perspectivePoint(u, v) {
        const point3 = {
          x: lerp(-halfW, halfW, u),
          y: lerp(-halfH, halfH, v),
          z: lerp(leftDepth, rightDepth, u),
        };
        return toPanel({ x: point3.x / point3.z, y: point3.y / point3.z });
      },
    };
  }

  function pathForLine(pointAt) {
    const points = [];
    for (let i = 0; i <= samplesPerLine; ++i) {
      points.push(pointAt(i / samplesPerLine));
    }
    return points.map((p, i) => `${i === 0 ? 'M' : 'L'} ${p.x.toFixed(2)} ${p.y.toFixed(2)}`).join(' ');
  }

  function drawGridLine(root, offsetX, pointAt, stroke, width) {
    root.appendChild(svg('path', {
      d: pathForLine(t => {
        const p = pointAt(t);
        return { x: offsetX + p.x, y: p.y };
      }),
      fill: 'none',
      stroke,
      'stroke-width': width,
      'stroke-linecap': 'round',
      'stroke-linejoin': 'round',
    }));
  }

  function drawPanel(root, projection, mode, offsetX, title) {
    root.appendChild(svg('text', {
      x: offsetX + 8, y: 18,
      'font-family': 'sans-serif', 'font-size': 14, 'font-weight': 'bold',
      fill: '#222',
    })).textContent = title;

    const point = mode === 'affine'
      ? projection.affinePoint
      : projection.perspectivePoint;
    root.appendChild(svg('polygon', {
      points: projection.verts.map(v => `${offsetX + v.x},${v.y}`).join(' '),
      fill: '#f7f3d0',
      'fill-opacity': 0.78,
    }));
    for (const t of gridSteps) {
      drawGridLine(root, offsetX, u => point(t, u), '#9dc0c4', 6);
      drawGridLine(root, offsetX, u => point(u, t), '#9dc0c4', 6);
    }
    for (const t of gridSteps) {
      drawGridLine(root, offsetX, u => point(t, u), '#246b8f', 2);
      drawGridLine(root, offsetX, u => point(u, t), '#246b8f', 2);
    }
    root.appendChild(svg('polygon', {
      points: projection.verts.map(v => `${offsetX + v.x},${v.y}`).join(' '),
      fill: 'none', stroke: '#111', 'stroke-width': 2,
    }));
  }

  function build(farZ) {
    const root = svg('svg', {
      width: panelW * 2 + gap,
      height: panelH,
      viewBox: `0 0 ${panelW * 2 + gap} ${panelH}`,
      style: 'background: #fafafa; user-select: none;',
    });
    const projection = makeProjection(farZ);
    drawPanel(root, projection, 'affine', 0, 'Affine screen-space UV');
    drawPanel(root, projection, 'perspective', panelW + gap, 'Perspective-correct UV');
    return root;
  }

  const container = document.createElement('div');
  const controls = document.createElement('div');
  controls.style.margin = '0 0 8px 0';
  controls.style.display = 'flex';
  controls.style.alignItems = 'center';
  controls.style.gap = '8px';
  const label = document.createElement('label');
  label.textContent = 'right edge depth';
  const slider = document.createElement('input');
  slider.type = 'range';
  slider.min = '1.05';
  slider.max = '6';
  slider.step = '0.1';
  slider.value = '3.2';
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
