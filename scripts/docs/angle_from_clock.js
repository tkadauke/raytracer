var AngleFromClock = new Class(AngleFromX, {
  createLabel: function() {
    return new Text(new Vector(3, -2), (this.radians * 1.909859317102744029227).toFixed(2) + " o'clock");
  },
  
  tick: function() {
    return 0.5235987755982988730771;
  }
});

(function(scriptElement) {
  var figure = new AngleFromClock();
  
  var handler = new DragHandler(figure);
  handler.handlerFunc = function(delta, figure) {
    figure.radians += delta.x * 0.033;
    return true;
  }
  
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.scripts[document.scripts.length - 1]);
