var BoxFarthestPoint = new Class({
  initialize: function() {
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.angle = 12 * degrees;
  },
  
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.center();

    var direction = Vector.up.rotated(this.angle);

    // plot direction vector
    canvas.add(new Line(Vector.null, direction, "arrow"));

    // plot the box
    canvas.add(new Rectangle(this.topleft, this.size));

    // plot the resulting point
    var point = this.farthestPoint(direction);
    canvas.add(new Circle(this.farthestPoint(direction), 0.05, "result"));
    
    // plot the tangential line
    var tangential = direction.rotated(90 * degrees);
    canvas.add(new Line(point.plus(tangential.multiply(-50)), tangential.multiply(100), "dashed"));
    
    return canvas.toSVG();
  },
  
  farthestPoint: function(direction) {
    return this.topleft.plus(new Vector(
      direction.x < 0 ? 0 : this.size.x,
      direction.y < 0 ? 0 : this.size.y
    ));
  },
});

(function(scriptElement) {
  var figure = new BoxFarthestPoint();

  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.angle += delta.x * degrees;
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
