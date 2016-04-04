var AngleFromRadians = new Class(AngleFromX, {
  createLabel: function() {
    return new Text(new Vector(3, -2), this.radians.toFixed(2) + " radians");
  },
  
  tick: function() {
    return 1;
  }
});

(function(scriptElement) {
  var figure = new AngleFromRadians();
  
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.radians += delta.x * 0.033;
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
