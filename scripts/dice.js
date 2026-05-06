// Six-sided die generator. Carved-out dots subtract from a beveled cube;
// the cube material is `diceMaterial`, the dot material is `dotMaterial`,
// and the dot radius is `dotSize`. Set those three on the ScriptedSurface
// and call create().
//
// Property bag picked up by ScriptedSurface::loadScript and exposed as
// editable Q_PROPERTY-style fields on the host element.
var properties = {
  diceMaterial: 'Material',
  dotMaterial: 'Material',
  dotSize: 'double'
}

function dice() {
  this.diceMaterial = null;
  this.dotMaterial = null;
  this.dotSize = 0.3;

  // Nested functions take `self` explicitly because under QJSEngine
  // (Qt 6) bare names don't fall through to the enclosing
  // constructor's `this`. The pre-Qt6 QtScript engine handled this
  // implicitly via `with`-like scoping; the explicit closure
  // pattern below is the standard JS workaround.

  this.createSphere = function(parent, vec) {
    var sphere = new Sphere(parent);
    sphere.radius = this.dotSize;
    sphere.position = vec;
  }

  this.createDots = function(parent, num, rotation) {
    var union = new Union(parent);
    union.name = "Face " + num;
    union.material = this.dotMaterial;
    union.rotation = rotation;

    if ([4, 5, 6].indexOf(num) != -1) {                    // X   o
      this.createSphere(union, new Vector3(-0.5, -0.5, -1.22)); // o o o
    }                                                      // o   o

    if ([6].indexOf(num) != -1) {                          // o   o
      this.createSphere(union, new Vector3(-0.5,  0.0, -1.22)); // X o o
    }                                                      // o   o

    if ([2, 3, 4, 5, 6].indexOf(num) != -1) {              // o   o
      this.createSphere(union, new Vector3(-0.5,  0.5, -1.22)); // o o o
    }                                                      // X   o

    if ([1, 3, 5].indexOf(num) != -1) {                    // o   o
      this.createSphere(union, new Vector3( 0.0,  0.0, -1.22)); // o X o
    }                                                      // o   o

    if ([2, 3, 4, 5, 6].indexOf(num) != -1) {              // o   X
      this.createSphere(union, new Vector3( 0.5, -0.5, -1.22)); // o o o
    }                                                      // o   o

    if ([6].indexOf(num) != -1) {                          // o   o
      this.createSphere(union, new Vector3( 0.5,  0.0, -1.22)); // o o X
    }                                                      // o   o

    if ([4, 5, 6].indexOf(num) != -1) {                    // o   o
      this.createSphere(union, new Vector3( 0.5,  0.5, -1.22)); // o o o
    }                                                      // o   X
  }

  this.create = function() {
    var dice = new Difference(this);
    var box = new Box(dice);
    box.bevelRadius = 0.15;
    box.material = this.diceMaterial;

    var degrees = 0.01745329251996;

    this.createDots(dice, 1, new Vector3(180 * degrees, 0, 0));
    this.createDots(dice, 2, new Vector3(90 * degrees, 0, 0));
    this.createDots(dice, 3, new Vector3(0, 90 * degrees, 0));
    this.createDots(dice, 4, new Vector3(0, 270 * degrees, 0));
    this.createDots(dice, 5, new Vector3(270 * degrees, 0, 0));
    this.createDots(dice, 6, new Vector3(0, 0, 0));
  }
}
