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
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
