var AngleFromDegrees = new Class(AngleFromX, {
  createLabel: function() {
    return new Text(new Vector(3, -2), (this.radians * 57.29577951308233).toFixed(2) + " degrees");
  },
  
  tick: function() {
    return 1.570796326794896619231;
  }
});

(function(scriptElement) {
  var figure = new AngleFromDegrees();
  
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.radians += delta.x * 0.033;
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
