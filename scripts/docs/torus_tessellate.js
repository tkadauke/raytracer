// Interactive widget for `Torus::tessellate` — two side-by-side
// views of the major × minor segmentation:
//
//   Left:  top-down view (looking down +Y, the hole axis). Shows
//          the major-circle segmentation as the polygonal outline
//          of the torus's outer rim. Each segment becomes one
//          column of quads in the full mesh.
//   Right: cross-section through the tube (the v-direction). Shows
//          the minor-circle segmentation as the polygonal outline
//          of one tube cross-section. Each segment becomes one row
//          of quads.
//
// Use the LOD slider to watch BOTH dimensions double. The full
// mesh has `majorSegs × minorSegs` quads — the product is what
// makes torus vertex counts grow fast (lod 0 → 289, lod 1 → 1089,
// lod 2 → 4225…).
//
// Teaching points:
//
//  - The torus has TWO seams, one per circle. Both are handled the
//    same way as the cylinder seam — by closing the row/column with
//    a duplicated final vertex carrying u=1 or v=1.
//  - Unlike the sphere, neither parametric direction degenerates at
//    a "pole" — every vertex in the grid is a regular interior
//    vertex with a well-defined normal. That's why the topology is
//    cleaner than the sphere's, even though the surface is more
//    complex.

const clampTorusLod = (value, min, max) => Math.max(min, Math.min(max, value));

class TorusTessellate {
  constructor() {
    this.lod = 0;       // 16 << lod major and minor segs
  }

  setLod(lod) {
    this.lod = clampTorusLod(Math.round(lod), 0, 3);
  }

  majorSegs() { return 16 << this.lod; }
  minorSegs() { return 16 << this.lod; }

  createCanvas() {
    // Canvas(440, 240) at the default 30 px/unit scale ≈ 14.67 × 8
    // user units. translate(0, -4) places user y = 0 at the
    // vertical center of the canvas.
    const canvas = new Canvas(440, 240);
    canvas.translate(new Vector(0, -4));

    const R = 1.4;        // swept radius (major)
    const r = 0.5;        // tube radius (minor)
    const majorSegs = this.majorSegs();
    const minorSegs = this.minorSegs();

    // ---- Left plot: top-down major ring ----
    const majorCenter = new Vector(3.5, 0);

    canvas.add(new Text(new Vector(2.0, -2.4), 'major ring (top-down)'));

    // True ring outline — outer + inner edges (radii R+r and R-r),
    // dashed for reference.
    canvas.add(new Circle(majorCenter, R + r, 'dashed'));
    canvas.add(new Circle(majorCenter, R - r, 'dashed'));

    // Polygon at radius R with majorSegs segments — the actual
    // tessellation of the major circle.
    const majorPts = [];
    for (let i = 0; i <= majorSegs; i++) {
      const theta = (2 * Math.PI * i) / majorSegs;
      majorPts.push(majorCenter.plus(
        new Vector(R * Math.cos(theta), R * Math.sin(theta))
      ));
    }
    for (let i = 0; i < majorSegs; i++) {
      canvas.add(new Line(majorPts[i], majorPts[i + 1].minus(majorPts[i])));
    }

    // ---- Right plot: minor ring (tube cross-section) ----
    const minorCenter = new Vector(10.0, 0);

    canvas.add(new Text(new Vector(8.5, -2.4), 'minor ring (tube cross-section)'));

    canvas.add(new Circle(minorCenter, r, 'dashed'));

    const minorPts = [];
    for (let j = 0; j <= minorSegs; j++) {
      const phi = (2 * Math.PI * j) / minorSegs;
      minorPts.push(minorCenter.plus(
        new Vector(r * Math.cos(phi), r * Math.sin(phi))
      ));
    }
    for (let j = 0; j < minorSegs; j++) {
      canvas.add(new Line(minorPts[j], minorPts[j + 1].minus(minorPts[j])));
    }

    // Counts — two stacked rows below the figures. Delta y = 1.0
    // user units (30 viewport px) is enough to clear the font.
    const vertexCount = (majorSegs + 1) * (minorSegs + 1);
    const quadCount = majorSegs * minorSegs;
    canvas.add(new Text(new Vector(2.0, 2.5),
      `major segs = minor segs = 16 << ${this.lod} = ${majorSegs}`));
    canvas.add(new Text(new Vector(2.0, 3.5),
      `vertices = ${majorSegs + 1}² = ${vertexCount}, quads = ${majorSegs}² = ${quadCount}`));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new TorusTessellate();

  const container = document.createElement('div');
  let canvas = figure.createCanvas();
  container.appendChild(canvas);

  const slider = new Slider({
    label: 'LOD',
    min: 0,
    max: 3,
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
