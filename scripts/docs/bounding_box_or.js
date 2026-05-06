class BoundingBoxOr {
  createCanvas() {
    const canvas = new Canvas(320, 240);

    canvas.add(new Rectangle(new Vector(2, -4), new Vector(5, 2), 'dashed'));
    canvas.add(new Rectangle(new Vector(4, -6), new Vector(4, 3), 'dashed'));
    canvas.add(new Rectangle(new Vector(2, -6), new Vector(6, 4)));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new BoundingBoxOr();
  scriptElement.parentNode.appendChild(figure.createCanvas());
})(document.currentScript);
