// Interactive widget for ThinLensCamera's class-level docstring —
// shows the concentric square-to-disc mapping that turns the active
// `ViewPlane` sampler's stratified [0,1]² sub-pixel offsets into
// stratified lens-disc samples.
//
// Two side-by-side plots:
//   Left:  N×N stratified grid in the unit square.
//   Right: same N×N points after the concentric mapping (Shirley 1997)
//          — they cover the unit disc uniformly with no rejection
//          and (importantly for ThinLensCamera) preserve the input
//          stratification.
//
// Drag horizontally to change N — bigger N → finer grid → smoother
// bokeh in the rendered output. The visible structure of the disc
// pattern (concentric rings + radial spokes) is what makes lens
// samples STRATIFIED rather than purely random; that's why bokeh
// converges at O(1/N) instead of O(1/√N).

function clamp(value, min, max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

// Shirley's "A Low Distortion Map Between Disk and Square" (1997).
// Same code as the C++ implementation in src/raytracer/cameras/ThinLensCamera.cpp.
function concentricMapToDisc(a, b) {
  if (a === 0 && b === 0) {
    return new Vector(0, 0);
  }
  var r, phi;
  if (a * a > b * b) {
    r = a;
    phi = (Math.PI / 4) * (b / a);
  } else {
    r = b;
    phi = (Math.PI / 2) - (Math.PI / 4) * (a / b);
  }
  return new Vector(r * Math.cos(phi), r * Math.sin(phi));
}

var ThinLensDiscSampling = new Class({
  initialize: function() {
    this.n = 6;       // grid is n × n samples
  },

  setN: function(n) {
    this.n = clamp(Math.round(n), 2, 20);
  },

  createCanvas: function() {
    var canvas = new Canvas(420, 220);
    canvas.translate(new Vector(0.7, -3.5));

    // Layout:
    //   square:  centred at (1.5, 0), side length 2.4
    //   arrow:   x ≈ 4.5
    //   disc:    centred at (7.5, 0), radius 1.2
    var squareCenter = new Vector(1.7, 0);
    var squareHalf = 1.2;
    var discCenter = new Vector(7.7, 0);
    var discRadius = 1.2;

    // Square outline
    canvas.add(new Rectangle(
      squareCenter.plus(new Vector(-squareHalf, -squareHalf)),
      new Vector(2 * squareHalf, 2 * squareHalf)
    ));
    canvas.add(new Text(squareCenter.plus(new Vector(-1.4, -1.5)),
                        "stratified [0,1]² (sampler input)"));

    // Disc outline
    canvas.add(new Circle(discCenter, discRadius));
    canvas.add(new Text(discCenter.plus(new Vector(-1.0, -1.5)),
                        "unit disc (lens sample)"));

    // Arrow between them
    canvas.add(new Line(new Vector(3.1, 0), new Vector(3.5, 0), "arrow"));
    canvas.add(new Text(new Vector(4.6, -0.2), "concentric"));
    canvas.add(new Text(new Vector(4.7, 0.4), "mapping"));

    // Plot N×N samples in both shapes.
    for (var x = 0; x < this.n; x++) {
      for (var y = 0; y < this.n; y++) {
        // Stratified grid sample in [0, 1)²
        var sx = (x + 0.5) / this.n;
        var sy = (y + 0.5) / this.n;

        // Square plot: scale (sx, sy) into the square box
        var sqPt = squareCenter.plus(
          new Vector((sx - 0.5) * 2 * squareHalf, (sy - 0.5) * 2 * squareHalf)
        );
        canvas.add(new Circle(sqPt, 0.04, "intersection"));

        // Map to [-1, 1]² then concentric → disc, then scale to disc plot
        var disc = concentricMapToDisc(2 * sx - 1, 2 * sy - 1);
        var discPt = discCenter.plus(disc.multiply(discRadius));
        canvas.add(new Circle(discPt, 0.04, "intersection"));
      }
    }

    return canvas.toSVG();
  }
});

// Slider control for `n`. Integer step so the grid density only
// takes whole-number values; precision: 0 in the label so it reads
// "n = 6" not "n = 6.00".
(function (scriptElement) {
  var figure = new ThinLensDiscSampling();

  var container = document.createElement("div");
  var canvas = figure.createCanvas();
  container.appendChild(canvas);

  var slider = new Slider({
    label: "grid density n",
    min: 2,
    max: 20,
    value: figure.n,
    step: 1,
    precision: 0,
    onChange: function (v) {
      figure.setN(v);
      var newCanvas = figure.createCanvas();
      container.replaceChild(newCanvas, canvas);
      canvas = newCanvas;
    }
  });
  container.appendChild(slider.element());

  scriptElement.parentNode.appendChild(container);
})(document.currentScript);
