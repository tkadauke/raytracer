class AngleFromClock extends AngleFromX {
  createLabel() {
    return new Text(new Vector(3, -2),
      `${(this.radians * 1.909859317102744029227).toFixed(2)} o'clock`);
  }

  tick() {
    return 0.5235987755982988730771;
  }
}

((scriptElement) => {
  const figure = new AngleFromClock();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.radians += delta.x * 0.033;
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
