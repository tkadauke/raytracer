// Interactive widget for ThinLensCamera's class-level docstring —
// illustrates the focal-plane convergence guarantee that's the load-
// bearing geometric insight of the whole class:
//
//   "Every ray for the same pixel — regardless of where on the lens
//   disc it starts — passes through the same point on the focal plane."
//
// Side-view layout, camera looks rightward:
//
//   eye/lens    image       focal
//   plane       plane       plane
//      |          |           |
//      |   ··· ─→ pixel ─→ ··· · in-focus point
//      |          |        ··· · (5 rays from lens disc all converging)
//      |          |           |
//
// Use the focalDistance slider to slide the focal plane. Watch:
// - The focal plane moves; the rays still converge at it.
// - The blur "fans" of rays past the focal plane open or close.
// - The pinhole reference ray (the one through the lens center) is
//   identical regardless of focalDistance — that's the chief ray that
//   all the off-axis rays converge to at the focal plane.

const clamp = (value, min, max) => Math.max(min, Math.min(max, value));

class ThinLensConvergence {
  constructor() {
    this.distance = 2.0;          // eye → image plane distance (m_distance)
    this.apertureRadius = 0.7;
    this.focalDistance = 4.0;     // image plane → focal plane distance
    this.pixelY = 0.45;           // pixel offset from optical axis
    this.numRays = 5;
  }

  setFocalDistance(d) {
    this.focalDistance = clamp(d, 1.0, 7.0);
  }

  createCanvas() {
    const canvas = new Canvas(420, 240);
    // x range: eye at x = -distance ≈ -2 → focal plane at x ≈ 1..7
    //          → margin → user x ∈ [-2.5, ~9]
    // y range: ±2.5 with axis at y=0
    canvas.translate(new Vector(2.5, -4));

    // Optical axis (horizontal line)
    canvas.add(new Line(new Vector(-this.distance - 0.4, 0),
                        new Vector(this.focalDistance + 1.7, 0),
                        "axis"));

    // Image plane (dashed vertical at x=0). Label sits ABOVE the
    // diagram so the focal-plane label can sit BELOW — that way the
    // two never collide as the user drags the focal plane closer to
    // or past the image plane.
    canvas.add(new Line(new Vector(0, -2.4), new Vector(0, 4.8), "dashed"));
    canvas.add(new Text(new Vector(-0.6, -2.6), "image plane"));

    // Focal plane (dashed vertical at x=focalDistance). Label below
    // the diagram for the reason above.
    canvas.add(new Line(new Vector(this.focalDistance, -2.4),
                        new Vector(0, 4.8),
                        "dashed red"));
    canvas.add(new Text(new Vector(this.focalDistance - 0.5, 2.8), "focal plane"));

    // Eye position
    const eye = new Vector(-this.distance, 0);

    // Aperture disc — vertical line spanning ±apertureRadius at x = -distance
    canvas.add(new Line(new Vector(-this.distance, -this.apertureRadius),
                        new Vector(0, 2 * this.apertureRadius)));
    canvas.add(new Text(new Vector(-this.distance + 0.15, -0.85), "lens"));

    // Pixel point on image plane (the "this is the pixel we're rendering" marker)
    const pixel = new Vector(0, this.pixelY);
    canvas.add(new Circle(pixel, 0.06, "intersection"));
    canvas.add(new Text(pixel.plus(new Vector(0.18, -0.8)), "pixel"));

    // Compute the focal point (intersection of the chief ray — eye-center
    // through pixel — with the focal plane). All rays from the lens disc
    // for THIS pixel must pass through this same point: that's the
    // convergence guarantee.
    const pinholeDir = pixel.minus(eye);
    // pinholeDir is the un-normalised direction (eye→pixel); to reach
    // x=focalDistance from eye, scale t such that eye.x + t*dir.x = focalDistance.
    const tFocal = (this.focalDistance - eye.x) / pinholeDir.x;
    const focalPoint = eye.plus(pinholeDir.multiply(tFocal));

    // N rays from points on the lens disc, all through focalPoint.
    // Each ray extends well past the focal plane to show the "fan-out"
    // for out-of-focus geometry.
    for (let i = 0; i < this.numRays; i++) {
      const t = this.numRays === 1 ? 0.5 : i / (this.numRays - 1);
      const lensY = -this.apertureRadius + t * 2 * this.apertureRadius;
      const lensPoint = new Vector(-this.distance, lensY);

      // Direction from lens point through focal point
      const rayDir = focalPoint.minus(lensPoint);
      // Extend the ray well past the focal plane (1.6× distance to focal)
      const ray = rayDir.multiply(1.6);
      canvas.add(new Line(lensPoint, ray));
    }

    // Mark the convergence point on the focal plane
    canvas.add(new Circle(focalPoint, 0.1, "result"));
    canvas.add(new Text(focalPoint.plus(new Vector(0.2, -0.75)), "in-focus point"));

    return canvas.toSVG();
  }
}

// Anchor the widget next to its own <script> tag. Uses
// document.currentScript (a stable browser API since ~2010) instead
// of the older document.scripts[length-1] trick, which was fragile
// against any future change in how Doxygen embeds the JS.
((scriptElement) => {
  const figure = new ThinLensConvergence();

  // A `Slider` (HTML range input with a live label) replaces the
  // earlier hidden horizontal drag affordance. The
  // user gets a clear range, a known control, and a numeric readout
  // — much better discoverability for a docs page.
  const container = document.createElement('div');
  let canvas = figure.createCanvas();
  container.appendChild(canvas);

  const slider = new Slider({
    label: 'focalDistance',
    min: 1.0,
    max: 7.0,
    value: figure.focalDistance,
    step: 0.1,
    precision: 1,
    onChange: (v) => {
      figure.setFocalDistance(v);
      const newCanvas = figure.createCanvas();
      container.replaceChild(newCanvas, canvas);
      canvas = newCanvas;
    }
  });
  container.appendChild(slider.element());

  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
