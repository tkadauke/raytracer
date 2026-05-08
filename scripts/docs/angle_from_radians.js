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
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
