function clamp(value, min, max) {
  if (value < min) {
    return min;
  } else if (value > max) {
    return max;
  }
  return value;
}

var RayClass = new Class({
  initialize: function() {
    this.origin = new Vector(4, -1);
    this.angle = 12 * degrees;
    this.length = 3;
    this.t = 0.5;
  },
  
  setT: function(t) {
    this.t = clamp(t, -1.0, 1.7);
  },
  
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.translate(new Vector(2, -2));

    canvas.add(new Axes());

    var direction = Vector.up.rotated(this.angle).multiply(this.length);

    // plot origin vector
    canvas.add(new Line(Vector.null, this.origin, "arrow"));

    // plot origin point
    canvas.add(new Circle(this.origin, 0.05, "intersection"));

    var ray = new Ray(this.origin, direction);

    // plot the ray
    canvas.add(ray);
    canvas.add(new Line(this.origin, direction, "arrow"));
    
    var point = ray.at(this.t);
    canvas.add(new Circle(point, 0.05, "result"));
    canvas.add(new Text(point.plus(new Vector(0.4, 0.2)), "t=" + this.t.toFixed(2)));
    
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new RayClass();

  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.setT(figure.t + delta.x / 100.0);
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
