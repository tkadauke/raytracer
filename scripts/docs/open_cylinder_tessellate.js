// Interactive widget for `OpenCylinder::tessellate` — top-down view
// of the side-surface tessellation, looking down the cylinder's Y
// axis. Drag the LOD slider to watch the segment count double at
// each level (16 → 32 → 64 → …). The teaching points:
//
//  - From above, the side-surface tessellation looks like a regular
//    polygon: each segment becomes one quad (a strip running from
//    the bottom rim to the top rim), so on this top-down projection
//    every quad collapses to a single rim edge.
//  - The seam at u=0/u=1 means there are `segments + 1` vertices per
//    rim, not `segments`. The first and last column share a position
//    but get distinct UVs (u=0 and u=1) so a wrapped texture doesn't
//    smear across the seam — the cost is one duplicated rim vertex
//    per ring.
//  - Normals point radially outward (drawn in red), so the side
//    surface shades smoothly across edges even at low LOD — the
//    polygon is silhouetted, not faceted in the lighting.

const clampCylLod = (value, min, max) => Math.max(min, Math.min(max, value));

class OpenCylinderTessellate {
  constructor() {
    this.lod = 0;       // 16 << lod segments
  }

  setLod(lod) {
    this.lod = clampCylLod(Math.round(lod), 0, 4);
  }

  segments() {
    return 16 << this.lod;
  }

  createCanvas() {
    const canvas = new Canvas(320, 320);
    canvas.center();

    const radius = 1.6;
    const segments = this.segments();

    // True circle for reference.
    canvas.add(new Circle(Vector.null, radius, 'dashed'));

    // Compute the `segments + 1` rim vertices (with seam duplication
    // at u=0 / u=1).
    const rim = [];
    for (let i = 0; i <= segments; i++) {
      const theta = (2 * Math.PI * i) / segments;
      rim.push(new Vector(radius * Math.cos(theta),
                          radius * Math.sin(theta)));
    }

    // Polygon edges (one per segment).
    for (let i = 0; i < segments; i++) {
      canvas.add(new Line(rim[i], rim[i + 1].minus(rim[i])));
    }

    // Outward normals at each segment midpoint, drawn at half-length
    // so they don't dominate the figure.
    for (let i = 0; i < segments; i++) {
      const theta = (2 * Math.PI * (i + 0.5)) / segments;
      const mid = new Vector(radius * Math.cos(theta),
                             radius * Math.sin(theta));
      const outward = new Vector(Math.cos(theta), Math.sin(theta));
      canvas.add(new Line(mid, outward.multiply(0.4), 'red'));
    }

    // Vertex/quad counts.
    canvas.add(new Text(new Vector(-2.2, 2.0),
      `segments = 16 << ${this.lod} = ${segments}`));
    canvas.add(new Text(new Vector(-2.2, 2.3),
      `vertices = 2 × (${segments} + 1) = ${2 * (segments + 1)}, quads = ${segments}`));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new OpenCylinderTessellate();

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
