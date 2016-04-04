var AngleFromTurns = new Class(AngleFromX, {
  createLabel: function() {
    return new Text(new Vector(3, -2), (this.radians * 0.1591549430918953358).toFixed(2) + " turns");
  },
  
  tick: function() {
    return 1.570796326794896619231;
  }
});

(function(scriptElement) {
  var figure = new AngleFromTurns();
  
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.radians += delta.x * 0.033;
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
