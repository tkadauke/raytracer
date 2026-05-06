class BoundingBoxInclude {
  constructor() {
    this.topleft = new Vector(-2, -2);
    this.size = new Vector(4, 4);
    this.point = new Vector(2.5, 1);
  }

  createCanvas() {
    const canvas = new Canvas(320, 240);
    canvas.center();

    // plot original box
    canvas.add(new Rectangle(this.topleft, this.size, 'dashed'));

    const newTopleft = new Vector(
      Math.min(this.point.x, this.topleft.x),
      Math.min(this.point.y, this.topleft.y)
    );
    const oldBottomRight = this.topleft.plus(this.size);
    const newBottomRight = new Vector(
      Math.max(this.point.x, oldBottomRight.x),
      Math.max(this.point.y, oldBottomRight.y)
    );
    const newSize = newBottomRight.minus(newTopleft);

    // plot resulting box
    canvas.add(new Rectangle(newTopleft, newSize));

    // plot the point
    canvas.add(new Circle(this.point, 0.05, 'result'));

    return canvas.toSVG();
  }
}

((scriptElement) => {
  const figure = new BoundingBoxInclude();
  const handler = new DragHandler(figure);
  handler.handlerFunc = (delta, figure) => {
    figure.point = figure.point.plus(delta.multiply(0.033));
    return true;
  };
  scriptElement.parentNode.appendChild(handler.divElement());
})(document.currentScript);
