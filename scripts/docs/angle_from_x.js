// Base class for the four angle-from-* widgets. Defines the unit
// circle, angle line, and tick marks; subclasses override
// `createLabel` and `tick` to choose the unit (radians, degrees,
// turns, o'clock).
//
// Declared via `var AngleFromX = class { ... }` rather than
// `class AngleFromX {}` so the binding attaches to `globalThis` —
// the four subclasses live in sibling files (angle_from_*.js) and
// reference `AngleFromX` by name, which only resolves if it's a
// global. Native `class Foo {}` declarations are block-scoped per
// spec and would not be visible across files.
var AngleFromX = class {
  constructor() {
    this.radians = 0.0;
    this.radius = 3;
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'angle-from-x-widget' });
    this.angleControl = new FigureSliderControl({
      label: 'angle',
      min: 0,
      max: 720,
      step: 1,
      value: this.radians / degrees,
      precision: 0,
      onChange: (value) => {
        this.radians = value * degrees;
        this.render();
      },
    });
    this.widget.addControl(this.angleControl.element());
    this.render();
    return this.widget.root;
  }

  createCanvas() {
    const canvas = new Canvas(320, 240);
    canvas.translate(new Vector(4, -4));

    // plot circle
    canvas.add(new Circle(Vector.null, this.radius));

    // plot angle
    const lineEnd = new Vector(0, -this.radius);
    canvas.add(new Line(Vector.null, lineEnd));
    canvas.add(new Line(Vector.null, lineEnd.rotated(this.radians)));

    canvas.add(this.createLabel());

    let tickAngle = 0;
    const tick = new Line(new Vector(0, -(this.radius - 0.2)), new Vector(0, 0.2));

    while (tickAngle < 2 * Math.PI) {
      const group = new Group();
      group.setTransform(`rotate(${(tickAngle * 57.29577951308233).toFixed(4)})`);
      group.add(tick);
      canvas.add(group);

      tickAngle += this.tick();
    }
    return canvas.toSVG();
  }

  // Default; subclasses override. Returns a `Text` element labelling
  // the current angle in the subclass's chosen unit.
  createLabel() {
    return new Text(new Vector(3, -2), `${this.radians.toFixed(2)} radians`);
  }

  render() {
    this.widget.setContent(this.createCanvas());
  }
};
