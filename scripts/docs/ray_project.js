var RayProject = new Class({
  initialize: function() {
    this.angle = 34 * degrees;
    this.numPoints = 8;
    this.origin = new Vector(0, 0);
  },
  
  createCanvas: function() {
    var direction = new Vector(0, -1).rotated(this.angle);

    var canvas = new Canvas(320, 240);
    canvas.translate(new Vector(5, -3));
    
    canvas.add(new Axes());

    // the line onto which we project the farthest points of all the circles
    var projection = new Ray(this.origin, direction, true);

    // plot the ray
    canvas.add(projection);

    for (var i = 0; i != this.numPoints; ++i) {
      center = new Vector(Math.random() * 10 - 5, -Math.random() * 8 + 3);
      canvas.add(new Circle(center, 0.05, "intersection"));
      
      var projected = projection.projected(center).minus(center);
      canvas.add(new Line(center, projected, "dashed"));
      canvas.add(new Circle(projected.plus(center), 0.05, "intersection"));
    }
    
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new RayProject();
  
  scriptElement.parentNode.appendChild(figure.createCanvas());
})(document.scripts[document.scripts.length - 1]);
