class AngleFromDegrees extends AngleFromX {
  createLabel() {
    return new Text(new Vector(3, -2),
      `${(this.radians * 57.29577951308233).toFixed(2)} degrees`);
  }

  tick() {
    return 1.570796326794896619231;
  }
}

((scriptElement) => {
  const figure = new AngleFromDegrees();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
