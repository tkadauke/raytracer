class BoundingBoxMovedBy {
  constructor() {
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.vector = new Vector(0.5, 1);
  }

  createCanvas() {
    const canvas = new Canvas(320, 240);
    canvas.center();

    // plot move vectors — one arrow per corner showing the
    // displacement uniformly applied to all four.
    canvas.add(new Line(this.topleft, this.vector, 'arrow'));
    canvas.add(new Line(this.topleft.plus(new Vector(this.size.x, 0)), this.vector, 'arrow'));
    canvas.add(new Line(this.topleft.plus(new Vector(0, this.size.y)), this.vector, 'arrow'));
    canvas.add(new Line(this.topleft.plus(this.size), this.vector, 'arrow'));

    // plot original box
    canvas.add(new Rectangle(this.topleft, this.size, 'dashed'));

    // plot moved box
    canvas.add(new Rectangle(this.topleft.plus(this.vector), this.size));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new BoundingBoxMovedBy();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.vector = figure.vector.plus(delta.multiply(0.033));
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
