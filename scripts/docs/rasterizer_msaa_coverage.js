// Interactive widget for the rasterizer's MSAA coverage and resolve step.
// Each pixel is shaded by the fraction of its subpixel samples covered by
// a triangle. 1x coverage is binary; 2x/4x/8x can resolve edge pixels to
// intermediate values.

(function(scriptElement) {
  const svgns = 'http://www.w3.org/2000/svg';
  const COLS = 9;
  const ROWS = 6;
  const CELL = 48;

  const samplePatterns = {
    1: [{ x: 0.0, y: 0.0 }],
    2: [
      { x: -0.25, y: -0.25 },
      { x:  0.25, y:  0.25 },
    ],
    4: [
      { x: -0.125, y: -0.375 },
      { x:  0.375, y: -0.125 },
      { x: -0.375, y:  0.125 },
      { x:  0.125, y:  0.375 },
    ],
    8: [
      { x:  0.0625, y: -0.1875 },
      { x: -0.0625, y:  0.1875 },
      { x:  0.3125, y:  0.0625 },
      { x: -0.1875, y: -0.3125 },
      { x: -0.3125, y:  0.3125 },
      { x: -0.4375, y: -0.0625 },
      { x:  0.1875, y:  0.4375 },
      { x:  0.4375, y: -0.4375 },
    ],
  };

  function svg(name, attrs) {
    const e = document.createElementNS(svgns, name);
    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    }
    return e;
  }

  function edge(a, b, p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
  }

  function triangle(edgeOffset) {
    return [
      { x: 1.0, y: 5.35 },
      { x: 8.35, y: 4.65 },
      { x: 1.25, y: 0.65 + edgeOffset },
    ];
  }

  function insideTriangle(p, tri) {
    const area = edge(tri[0], tri[1], tri[2]);
    if (Math.abs(area) < 1e-9) return false;
    const w0 = edge(tri[1], tri[2], p);
    const w1 = edge(tri[2], tri[0], p);
    const w2 = area - w0 - w1;
    return area > 0
      ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
      : (w0 <= 0 && w1 <= 0 && w2 <= 0);
  }

  function shade(covered, total) {
    const v = Math.round((covered / total) * 255);
    return `rgb(${v}, ${v}, ${v})`;
  }

  function build(sampleCount, edgeOffset) {
    const pattern = samplePatterns[sampleCount];
    const tri = triangle(edgeOffset);
    const root = svg('svg', {
      width: COLS * CELL,
      height: ROWS * CELL + 34,
      viewBox: `0 0 ${COLS * CELL} ${ROWS * CELL + 34}`,
      style: 'background: #fafafa; user-select: none;',
    });

    for (let py = 0; py < ROWS; py++) {
      for (let px = 0; px < COLS; px++) {
        let covered = 0;
        for (const offset of pattern) {
          if (insideTriangle({
            x: px + 0.5 + offset.x,
            y: py + 0.5 + offset.y,
          }, tri)) {
            covered++;
          }
        }

        root.appendChild(svg('rect', {
          x: px * CELL,
          y: py * CELL,
          width: CELL,
          height: CELL,
          fill: shade(covered, pattern.length),
          stroke: '#d5d5d5',
          'stroke-width': 1,
          'data-covered-samples': covered,
          'data-sample-count': pattern.length,
        }));

        if (covered > 0 && covered < pattern.length) {
          const label = svg('text', {
            x: px * CELL + CELL / 2,
            y: py * CELL + CELL / 2 + 4,
            'font-family': 'monospace',
            'font-size': 11,
            'text-anchor': 'middle',
            fill: covered > pattern.length / 2 ? '#111' : '#fff',
            'pointer-events': 'none',
          });
          label.textContent = `${covered}/${pattern.length}`;
          root.appendChild(label);
        }

        for (const offset of pattern) {
          const samplePoint = {
            x: px + 0.5 + offset.x,
            y: py + 0.5 + offset.y,
          };
          const hit = insideTriangle(samplePoint, tri);
          root.appendChild(svg('circle', {
            cx: samplePoint.x * CELL,
            cy: samplePoint.y * CELL,
            r: Math.max(2.5, 6 - pattern.length * 0.35),
            fill: hit ? '#0b7285' : '#ffffff',
            stroke: hit ? '#083f4a' : '#9b9b9b',
            'stroke-width': 1.25,
            'data-sample-hit': hit ? '1' : '0',
          }));
        }
      }
    }

    root.appendChild(svg('polygon', {
      points: tri.map(v => `${v.x * CELL},${v.y * CELL}`).join(' '),
      fill: 'none',
      stroke: '#111',
      'stroke-width': 3,
      'stroke-linejoin': 'round',
    }));

    const caption = svg('text', {
      x: 0,
      y: ROWS * CELL + 23,
      'font-family': 'sans-serif',
      'font-size': 14,
      fill: '#333',
    });
    caption.textContent = `${sampleCount}x MSAA: pixel colour = covered samples / ${sampleCount}`;
    root.appendChild(caption);

    return root;
  }

  const container = document.createElement('div');
  container.style.maxWidth = `${COLS * CELL}px`;

  const controls = document.createElement('div');
  controls.style.margin = '0 0 8px 0';
  controls.style.display = 'flex';
  controls.style.alignItems = 'center';
  controls.style.gap = '8px';
  controls.style.flexWrap = 'wrap';

  const sampleLabel = document.createElement('span');
  sampleLabel.textContent = 'samples';
  controls.appendChild(sampleLabel);

  let sampleCount = 4;
  const sampleButtons = [1, 2, 4, 8].map(count => {
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = `${count}x`;
    button.style.padding = '4px 8px';
    button.style.border = '1px solid #aaa';
    button.style.borderRadius = '4px';
    button.style.background = count === sampleCount ? '#111' : '#fff';
    button.style.color = count === sampleCount ? '#fff' : '#111';
    button.addEventListener('click', () => {
      sampleCount = count;
      updateButtons();
      replaceSvg();
    });
    controls.appendChild(button);
    return button;
  });

  const edgeLabel = document.createElement('label');
  edgeLabel.textContent = 'edge position';
  controls.appendChild(edgeLabel);

  const slider = document.createElement('input');
  slider.type = 'range';
  slider.min = '-0.8';
  slider.max = '1.2';
  slider.step = '0.05';
  slider.value = '0';
  controls.appendChild(slider);

  const value = document.createElement('span');
  value.textContent = '0.00';
  controls.appendChild(value);

  container.appendChild(controls);

  function updateButtons() {
    sampleButtons.forEach((button, index) => {
      const active = [1, 2, 4, 8][index] === sampleCount;
      button.style.background = active ? '#111' : '#fff';
      button.style.color = active ? '#fff' : '#111';
    });
  }

  let svgEl = build(sampleCount, parseFloat(slider.value));
  container.appendChild(svgEl);

  function replaceSvg() {
    value.textContent = parseFloat(slider.value).toFixed(2);
    const next = build(sampleCount, parseFloat(slider.value));
    container.replaceChild(next, svgEl);
    svgEl = next;
  }

  slider.addEventListener('input', replaceSvg);
  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
