var BoudingBoxMovedBy = new Class({
  initialize: function() {
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.vector = new Vector(0.5, 1);
  },
  
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.center();

    // plot move vectors
    canvas.add(new Line(this.topleft, this.vector, "arrow"));
    canvas.add(new Line(this.topleft.plus(new Vector(this.size.x, 0)), this.vector, "arrow"));
    canvas.add(new Line(this.topleft.plus(new Vector(0, this.size.y)), this.vector, "arrow"));
    canvas.add(new Line(this.topleft.plus(this.size), this.vector, "arrow"));

    // plot original box
    canvas.add(new Rectangle(this.topleft, this.size, "dashed"));
    
    // plot moved box
    canvas.add(new Rectangle(this.topleft.plus(this.vector), this.size));
    
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new BoudingBoxMovedBy();
  
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.vector = figure.vector.plus(delta.multiply(0.033));
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
