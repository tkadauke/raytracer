// Interactive widget for TiltShiftCamera's class-level docstring —
// shows what tilt actually *does* geometrically: it rotates the focal
// plane off-perpendicular to the forward axis, so different parts of
// the scene at different depths can be in focus simultaneously.
//
// Side-view layout, camera looks rightward (same conventions as the
// ThinLens convergence widget; the only changes are the rotated focal
// plane and the per-pixel-row focal points):
//
//   eye/lens    image       focal plane (TILTED)
//   plane       plane            \
//      |          |               \   ← rotates with the slider
//      |   ··· ─→ pixel A ─→ ···   \
//      |          |        ···  ·  · in-focus point for pixel A
//      |   ··· ─→ pixel B ─→ ···  \
//      |          |        ···  ·  · in-focus point for pixel B (different!)
//      |          |                  \
//
// Key insight the widget reinforces: with tilt > 0, *different pixels
// have focal points at different depths*. Pixel A above the optical
// axis focuses farther; pixel B below focuses closer. That's why a
// horizontal table can be sharp end-to-end (matching the tilted
// focal plane) and why the miniature effect compresses focus to a
// thin band when the tilt is steep.
//
// Drag the slider to rotate the focal plane. At tilt = 0 the widget
// degenerates to the ThinLens convergence diagram (same focal point
// for every pixel). At tilt = 30° the focal plane visibly leans, and
// the in-focus points for the upper and lower pixels separate.
//
// Note: top-level `const`s would collide with `thin_lens_camera_convergence.js`'s
// `clamp` declaration when Doxygen loads both scripts onto the same
// page (and when the Node-side smoke test loads them into a shared
// sandbox). Inline the clamp directly to keep the global namespace
// clear of single-letter helpers.

class ScheimpflugConvergence {
  constructor() {
    this.distance = 2.0;          // eye → image plane distance
    this.apertureRadius = 0.7;
    this.focalDistance = 4.0;     // along forward axis
    this.tiltDeg = 20;            // rotation of focal plane around right axis
    this.pixelYs = [0.6, -0.6];   // two pixels — one above, one below axis
    this.numRays = 5;
  }

  setTilt(deg) {
    this.tiltDeg = Math.max(-45, Math.min(45, deg));
  }

  // Compute the focal point for a given pixel, intersecting the eye→pixel
  // ray with the tilted focal plane. The plane's normal is `forward`
  // rotated around the +x (right) axis by tiltDeg. In 2D side view,
  // forward = (1, 0) and the rotated normal is
  //   (cos t, -sin t)
  // (negative sin because tilting the top of the plane toward the camera
  //  rotates the normal "down" in the y direction). The plane passes
  // through (focalDistance, 0).
  focalPointFor(pixelY) {
    const eye = { x: -this.distance, y: 0 };
    const pixel = { x: 0, y: pixelY };
    const dir = { x: pixel.x - eye.x, y: pixel.y - eye.y };

    const tRad = this.tiltDeg * Math.PI / 180;
    const nx = Math.cos(tRad);
    const ny = -Math.sin(tRad);

    // Plane: n · (P - P0) = 0, P0 = (focalDistance, 0).
    // Ray: P = eye + t * dir.  Solve for t:
    //   n · (eye + t*dir - P0) = 0
    //   t = (n · (P0 - eye)) / (n · dir)
    const num = nx * (this.focalDistance - eye.x) + ny * (0 - eye.y);
    const den = nx * dir.x + ny * dir.y;
    const t = num / den;
    return { x: eye.x + t * dir.x, y: eye.y + t * dir.y };
  }

  createCanvas() {
    const canvas = new Canvas(420, 260);
    canvas.translate(new Vector(2.5, -4.5));

    // Optical axis
    canvas.add(new Line(new Vector(-this.distance - 0.4, 0),
                        new Vector(this.focalDistance + 2.5, 0),
                        "axis"));

    // Image plane (dashed vertical at x=0)
    canvas.add(new Line(new Vector(0, -2.8), new Vector(0, 5.6), "dashed"));
    canvas.add(new Text(new Vector(-0.6, -3.0), "image plane"));

    // Tilted focal plane line. The focal-plane normal is
    // `(cos t, -sin t)`; perpendicular to that — i.e. a direction
    // along the line — is `(sin t, cos t)`. Step by ±(sin t, cos t)
    // * half from the anchor point `(focalDistance, 0)`. Going
    // "up" visually means decreasing canvas y (canvas y is flipped),
    // so the top endpoint subtracts the line direction:
    //
    //   fpTop    = focalDistance + (-sin t, -cos t) * half
    //   fpBottom = focalDistance + ( sin t,  cos t) * half
    //
    // Earlier versions had the wrong sign on the x-component, which
    // made the drawn line lean the *opposite* way to the focal-point
    // calculation, leaving the red dots floating off the line.
    const tRad = this.tiltDeg * Math.PI / 180;
    const sinT = Math.sin(tRad);
    const cosT = Math.cos(tRad);
    const half = 2.6;
    const fpTop    = new Vector(this.focalDistance - sinT * half, -cosT * half);
    const fpBottom = new Vector(this.focalDistance + sinT * half,  cosT * half);
    canvas.add(new Line(fpTop, fpBottom.minus(fpTop), "dashed red"));
    canvas.add(new Text(fpTop.plus(new Vector(-0.5, -0.3)), "focal plane"));

    const eye = new Vector(-this.distance, 0);

    // Aperture
    canvas.add(new Line(new Vector(-this.distance, -this.apertureRadius),
                        new Vector(0, 2 * this.apertureRadius)));
    canvas.add(new Text(new Vector(-this.distance + 0.15, -0.85), "lens"));

    // For each of the two pixels: draw the pixel marker, compute its
    // focal point on the tilted plane, and fan out N lens-disc rays
    // through that focal point.
    const pixelLabels = ["pixel A", "pixel B"];
    for (let p = 0; p < this.pixelYs.length; p++) {
      const pixelY = this.pixelYs[p];
      const pixel = new Vector(0, pixelY);
      const focal = new Vector(...Object.values(this.focalPointFor(pixelY)));

      canvas.add(new Circle(pixel, 0.06, "intersection"));
      canvas.add(new Text(new Vector(0.28, pixelY > 0 ? 1.25 : -1.25), pixelLabels[p]));

      for (let i = 0; i < this.numRays; i++) {
        const t = this.numRays === 1 ? 0.5 : i / (this.numRays - 1);
        const lensY = -this.apertureRadius + t * 2 * this.apertureRadius;
        const lensPoint = new Vector(-this.distance, lensY);
        const rayDir = focal.minus(lensPoint);
        canvas.add(new Line(lensPoint, rayDir.multiply(1.4)));
      }

      canvas.add(new Circle(focal, 0.1, "result"));
    }

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new ScheimpflugConvergence();

  const container = document.createElement('div');
  let canvas = figure.createCanvas();
  container.appendChild(canvas);

  const slider = new Slider({
    label: 'tilt (degrees)',
    min: -45,
    max: 45,
    value: figure.tiltDeg,
    step: 1,
    precision: 0,
    onChange: (v) => {
      figure.setTilt(v);
      const newCanvas = figure.createCanvas();
      container.replaceChild(newCanvas, canvas);
      canvas = newCanvas;
    }
  });
  container.appendChild(slider.element());

  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
