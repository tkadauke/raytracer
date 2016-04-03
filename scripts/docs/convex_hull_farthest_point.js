var ConvexHullFarthestPoint = new Class({
  initialize: function() {
    this.angle = 162 * degrees;
  },
  
  createCanvas: function() {
    var direction = Vector.up.rotated(this.angle);

    var canvas = new Canvas(320, 240);
    canvas.translate(new Vector(2, -1));
    
    canvas.add(new Axes());

    // the line onto which we project the farthest points of all the circles
    var projection = new Ray(Vector.null, direction, true);

    // plot the ray
    canvas.add(projection);

    // this ordered hash holds all the farthest points for all the circles
    var distances = new OrderedHash();
    
    [
      new Vector(2, -3),
      new Vector(6, -4),
      new Vector(4, -5)
    ].forEach(function(center) {
      canvas.add(new Circle(center, 1));
      canvas.add(new Line(center, direction, "arrow"));
      
      var farthest = center.plus(direction);
      canvas.add(new Circle(farthest, 0.05, "intersection"));
      
      var distance = projection.projectedDistance(farthest);
      distances.push(distance, farthest);
      
      var projected = projection.at(distance).minus(farthest);
      canvas.add(new Line(farthest, projected, "dashed"));
      canvas.add(new Circle(projection.at(distance), 0.05, "intersection"));
    });
    
    // calculate the farthest point
    var keys = distances.sortedKeys();
    var point = distances.get(keys[keys.length - 1]);
    canvas.add(new Circle(point, 0.05, "result"))
    
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new ConvexHullFarthestPoint();
  
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.angle += delta.x * degrees;
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
