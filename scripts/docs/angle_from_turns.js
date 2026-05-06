class AngleFromTurns extends AngleFromX {
  createLabel() {
    return new Text(new Vector(3, -2),
      `${(this.radians * 0.1591549430918953358).toFixed(2)} turns`);
  }

  tick() {
    return 1.570796326794896619231;
  }
}

((scriptElement) => {
  const figure = new AngleFromTurns();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.radians += delta.x * 0.033;
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
