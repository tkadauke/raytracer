class BoundingBoxAnd {
  createCanvas() {
    const canvas = new Canvas(320, 240);

    canvas.add(new Rectangle(new Vector(2, -4), new Vector(5, 2), 'dashed'));
    canvas.add(new Rectangle(new Vector(4, -6), new Vector(4, 3), 'dashed'));
    canvas.add(new Rectangle(new Vector(4, -4), new Vector(3, 1)));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new BoundingBoxAnd();
  scriptElement.parentNode.appendChild(figure.createCanvas());
})(document.currentScript);
