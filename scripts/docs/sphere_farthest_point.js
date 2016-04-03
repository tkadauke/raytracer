var SphereFarthestPoint = new Class({
  initialize: function() {
    this.radius = 2
    this.angle = 12 * degrees;
  },
  
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.center();

    var direction = Vector.up.rotated(this.angle);

    // plot direction vector
    canvas.add(new Line(Vector.null, direction, "arrow"));

    // plot the box
    canvas.add(new Circle(Vector.null, this.radius));

    // plot the resulting point
    var point = this.farthestPoint(direction);
    canvas.add(new Circle(point, 0.05, "result"));
    
    // plot the tangential line
    var tangential = direction.rotated(90 * degrees);
    canvas.add(new Line(point.plus(tangential.multiply(-50)), tangential.multiply(100), "dashed"));
    
    return canvas.toSVG();
  },
  
  farthestPoint: function(direction) {
    return direction.multiply(this.radius);
  }
});

(function(scriptElement) {
  var figure = new SphereFarthestPoint();

  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.angle += delta.x * degrees;
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
