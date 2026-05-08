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
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
