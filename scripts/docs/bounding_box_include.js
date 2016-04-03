var BoudingBoxInclude = new Class({
  initialize: function() {
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.point = new Vector(2.5, 1);
  },
  
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    canvas.center();

    // plot original box
    canvas.add(new Rectangle(this.topleft, this.size, "dashed"));
    
    var newTopleft = new Vector(
      Math.min(this.point.x, this.topleft.x),
      Math.min(this.point.y, this.topleft.y)
    );
    var oldBottomRight = this.topleft.plus(this.size);
    var newBottomRight = new Vector(
      Math.max(this.point.x, oldBottomRight.x),
      Math.max(this.point.y, oldBottomRight.y)
    );
    var newSize = newBottomRight.minus(newTopleft);
    
    // plot resulting box
    canvas.add(new Rectangle(newTopleft, newSize));
    
    // plot the point
    canvas.add(new Circle(this.point, 0.05, "result"));
    
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new BoudingBoxInclude();
  
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.point = figure.point.plus(delta.multiply(0.033));
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
