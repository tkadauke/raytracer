// Interactive widget for `Disk::tessellate` — top-down view of the
// triangle-fan layout the tessellator produces. Use the LOD slider
// to watch the segment count double at each level (16 → 32 → 64 →
// …). The teaching points the static image table can't show:
//
//  - Every triangle shares the center vertex (index 0). That's why
//    the fan layout has `1 + segments` vertices and `segments`
//    triangles, not `2 * segments` of either.
//  - Doubling the segment count halves the angular gap on the rim,
//    quartering the gap area between the polygon and the true circle
//    (since the chord-vs-arc gap goes as θ³).
//  - The polygon visibly shrinks toward the inscribed circle as the
//    segment count rises — that's the geometric quality the LOD
//    parameter is buying you, at a linear cost in vertex/triangle
//    count.

const clampLod = (value, min, max) => Math.max(min, Math.min(max, value));

class DiskTessellate {
  constructor() {
    this.lod = 0;       // 16 << lod segments
  }

  setLod(lod) {
    this.lod = clampLod(Math.round(lod), 0, 4);
  }

  segments() {
    return 16 << this.lod;
  }

  createCanvas() {
    // Canvas(320, 240) is the geometry `Canvas#center()` is
    // calibrated for — it translates by (5.5, -4) which puts user
    // (0, 0) at the viewport center (160, 120).
    const canvas = new Canvas(320, 240);
    canvas.center();

    const radius = 1.6;
    const segments = this.segments();

    // True circle (the target). Drawn dashed for reference so the
    // viewer can see how closely the polygon approximates it.
    canvas.add(new Circle(Vector.null, radius, 'dashed'));

    // Rim vertices around the circle, plus the center.
    const rim = [];
    for (let i = 0; i < segments; i++) {
      const theta = (2 * Math.PI * i) / segments;
      rim.push(new Vector(radius * Math.cos(theta),
                          radius * Math.sin(theta)));
    }

    // The triangle-fan edges: from center to each rim vertex (the
    // "spokes") and along the rim itself (the polygon outline).
    for (let i = 0; i < segments; i++) {
      // Spoke from center to rim[i]
      canvas.add(new Line(Vector.null, rim[i]));
      // Polygon edge from rim[i] to rim[i+1]
      const next = rim[(i + 1) % segments];
      canvas.add(new Line(rim[i], next.minus(rim[i])));
    }

    // Centre vertex (index 0) — drawn distinctly to highlight that
    // it's shared by every triangle in the fan.
    canvas.add(new Circle(Vector.null, 0.06, 'result'));

    // Vertex/triangle count — the load-bearing numbers per the LOD.
    // Delta y = 1.0 user units (30 viewport px) is enough to clear
    // the font.
    canvas.add(new Text(new Vector(-3.0, 2.5),
      `segments = 16 << ${this.lod} = ${segments}`));
    canvas.add(new Text(new Vector(-3.0, 3.5),
      `vertices = ${1 + segments}, triangles = ${segments}`));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new DiskTessellate();

  const container = document.createElement('div');
  let canvas = figure.createCanvas();
  container.appendChild(canvas);

  const slider = new Slider({
    label: 'LOD',
    min: 0,
    max: 4,
    value: figure.lod,
    step: 1,
    precision: 0,
    onChange: (v) => {
      figure.setLod(v);
      const newCanvas = figure.createCanvas();
      container.replaceChild(newCanvas, canvas);
      canvas = newCanvas;
    }
  });
  container.appendChild(slider.element());

  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
