class AngleFromRadians extends AngleFromX {
  createLabel() {
    return new Text(new Vector(3, -2), `${this.radians.toFixed(2)} radians`);
  }

  tick() {
    return 1;
  }
}

((scriptElement) => {
  const figure = new AngleFromRadians();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.radians += delta.x * 0.033;
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
