var AngleFromX = new Class({
  initialize: function() {
    this.radians = 0.0;
    this.radius = 3;
  },
  
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.translate(new Vector(4, -4));

    // plot circle
    canvas.add(new Circle(Vector.null, this.radius));
    
    // plot angle
    var lineEnd = new Vector(0, -this.radius);
    canvas.add(new Line(Vector.null, lineEnd));
    canvas.add(new Line(Vector.null, lineEnd.rotated(this.radians)));
    
    canvas.add(this.createLabel());
    
    var tickAngle = 0;
    var tick = new Line(new Vector(0, -(this.radius-0.2)), new Vector(0, 0.2));
    
    while (tickAngle < 2*Math.PI) {
      var group = new Group();
      group.setTransform("rotate(" + (tickAngle * 57.29577951308233).toFixed(4) + ")");
      group.add(tick);
      canvas.add(group);
      
      tickAngle += this.tick();
    }
    return canvas.toSVG();
  },
  
  createLabel: function() {
    canvas.add(new Text(new Vector(3, -2), this.radians.toFixed(2) + " radians"));
  }
});
