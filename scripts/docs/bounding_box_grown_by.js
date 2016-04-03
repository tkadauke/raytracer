var BoudingBoxGrownBy = new Class({
  initialize: function() {
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.vector = new Vector(0.5, 1);
  },
  
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.center();

    // plot move vectors
    canvas.add(new Line(this.topleft, this.vector.multiply(-1), "arrow"));
    canvas.add(new Line(this.topleft.plus(new Vector(this.size.x, 0)), new Vector(this.vector.x, -this.vector.y), "arrow red"));
    canvas.add(new Line(this.topleft.plus(new Vector(0, this.size.y)), new Vector(-this.vector.x, this.vector.y), "arrow"));
    canvas.add(new Line(this.topleft.plus(this.size), this.vector, "arrow"));

    // plot original box
    canvas.add(new Rectangle(this.topleft, this.size, "dashed"));
    
    // plot grown box
    canvas.add(new Rectangle(this.topleft.minus(this.vector), this.size.plus(this.vector.multiply(2))));
    
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new BoudingBoxGrownBy();
  
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.vector = figure.vector.plus(new Vector(delta.x * 0.033, delta.y * -0.033));
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
