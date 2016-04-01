var RayClass = new Class({
  initialize: function() {
    this.origin = new Vector(4, -1);
    this.angle = 12 * degrees;
    this.length = 3;
  },
  
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.translate(new Vector(2, -2));

    canvas.add(new Axes());

    var direction = new Vector(0, -1).rotated(this.angle).multiply(this.length);

    // plot origin vector
    canvas.add(new Line(new Vector(0, 0), this.origin, "arrow"));

    // plot origin point
    canvas.add(new Circle(this.origin, 0.05, "intersection"));
    canvas.add(new Text(this.origin.multiply(0.5).plus(new Vector(0, -0.2)), "o"));

    var ray = new Ray(this.origin, direction);

    // plot the ray
    canvas.add(ray);
    canvas.add(new Line(this.origin, direction, "arrow"));
    canvas.add(new Text(ray.at(0.5).plus(new Vector(-0.5, 0.3)), "d"));
    
    canvas.add(new Text(ray.at(0).plus(new Vector(0.4, 0.2)), "t=0"));
    canvas.add(new Text(ray.at(1).plus(new Vector(0.4, 0.2)), "t=1"));
    
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new RayClass();
  
  scriptElement.parentNode.appendChild(figure.createCanvas());
})(document.scripts[document.scripts.length - 1]);
