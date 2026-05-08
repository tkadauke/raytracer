// Interactive widget for `Sphere::tessellate` — side view (looking
// down +X) of the UV-sphere parametrisation, showing latitude bands
// stacked from south pole to north pole. Use the LOD slider to
// watch the band count double at each level (8 → 16 → 32 → …).
//
// Teaching points the static image table can't show:
//
//  - Bands aren't equal-area: the equatorial band covers far more
//    surface area than a polar band of the same angular height.
//    That's the inherent inefficiency of a UV sphere — alternative
//    parametrisations (geodesic / icosphere) trade UV simplicity for
//    more uniform sampling.
//  - The pole vertex is "duplicated" (`lonSegs + 1` copies sharing
//    the same 3D position with distinct u-values). On this side
//    view they collapse onto a single point, but they live as
//    separate vertices in the mesh — that's what keeps the texture
//    seam from pinching to a singularity.
//  - LOD doubles BOTH dimensions, so vertex count grows ~4× per
//    level (lod 0 → 153, lod 1 → 561, lod 2 → 2145…).

const clampSphereLod = (value, min, max) => Math.max(min, Math.min(max, value));

class SphereTessellate {
  constructor() {
    this.lod = 0;       // 8 << lod latBands, 16 << lod lonSegs
  }

  setLod(lod) {
    this.lod = clampSphereLod(Math.round(lod), 0, 3);
  }

  latBands() { return 8 << this.lod; }
  lonSegs()  { return 16 << this.lod; }

  createCanvas() {
    // Canvas(320, 240) is the geometry `Canvas#center()` is
    // calibrated for — it translates by (5.5, -4) which puts user
    // (0, 0) at the viewport center (160, 120).
    const canvas = new Canvas(320, 240);
    canvas.center();

    const radius = 1.6;
    const latBands = this.latBands();
    const lonSegs = this.lonSegs();

    // Reference circle (the silhouette in this view).
    canvas.add(new Circle(Vector.null, radius, 'dashed'));

    // Latitude rings — each is a horizontal chord. lat=0 is the
    // south pole (single point), lat=latBands is the north pole.
    for (let lat = 0; lat <= latBands; lat++) {
      const theta = -Math.PI / 2 + (lat * Math.PI) / latBands;
      const y = -radius * Math.sin(theta);   // SVG y is flipped
      const halfChord = radius * Math.cos(theta);
      // The chord — visually a horizontal line connecting the two
      // edge vertices of this latitude band. Polar chords degenerate
      // to a point.
      canvas.add(new Line(
        new Vector(-halfChord, y),
        new Vector(2 * halfChord, 0)
      ));
    }

    // Vertices on the silhouette circle — one per latitude band on
    // each side. These are the lon=0 and lon=lonSegs/2 vertices,
    // i.e. the seam edge in this projection.
    for (let lat = 0; lat <= latBands; lat++) {
      const theta = -Math.PI / 2 + (lat * Math.PI) / latBands;
      const y = -radius * Math.sin(theta);
      const halfChord = radius * Math.cos(theta);
      canvas.add(new Circle(new Vector(-halfChord, y), 0.04, 'intersection'));
      canvas.add(new Circle(new Vector( halfChord, y), 0.04, 'intersection'));
    }

    // Pole markers — drawn in red because they're the topology-
    // surprising vertices (one position, lonSegs+1 vertices).
    canvas.add(new Circle(new Vector(0,  radius), 0.06, 'result'));
    canvas.add(new Circle(new Vector(0, -radius), 0.06, 'result'));

    // Counts. Delta y = 1.0 user units (30 viewport px) is enough
    // to clear the font.
    const vertexCount = (latBands + 1) * (lonSegs + 1);
    const quadCount = latBands * lonSegs;
    canvas.add(new Text(new Vector(-3.0, 2.5),
      `lat bands = ${latBands}, lon segs = ${lonSegs}`));
    canvas.add(new Text(new Vector(-3.0, 3.5),
      `vertices = ${vertexCount}, quads = ${quadCount}`));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new SphereTessellate();

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
