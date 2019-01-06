var properties = {
  diceMaterial: 'Material',
  dotMaterial: 'Material',
  dotSize: 'double'
}

function dice() {
  this.diceMaterial = null;
  this.dotMaterial = null;
  this.dotSize = 0.3;

  function createDots(parent, num, rotation) {
    var union = new Union(parent);
    union.name = "Face " + num;
    union.material = dotMaterial;
    union.rotation = rotation;

    if ([4, 5, 6].indexOf(num) != -1) {                    // X   o
      createSphere(union, new Vector3(-0.5, -0.5, -1.22)); // o o o
    }                                                      // o   o

    if ([6].indexOf(num) != -1) {                          // o   o
      createSphere(union, new Vector3(-0.5,  0.0, -1.22)); // X o o
    }                                                      // o   o

    if ([2, 3, 4, 5, 6].indexOf(num) != -1) {              // o   o
      createSphere(union, new Vector3(-0.5,  0.5, -1.22)); // o o o
    }                                                      // X   o

    if ([1, 3, 5].indexOf(num) != -1) {                    // o   o
      createSphere(union, new Vector3( 0.0,  0.0, -1.22)); // o X o
    }                                                      // o   o

    if ([2, 3, 4, 5, 6].indexOf(num) != -1) {              // o   X
      createSphere(union, new Vector3( 0.5, -0.5, -1.22)); // o o o
    }                                                      // o   o

    if ([6].indexOf(num) != -1) {                          // o   o
      createSphere(union, new Vector3( 0.5,  0.0, -1.22)); // o o X
    }                                                      // o   o

    if ([4, 5, 6].indexOf(num) != -1) {                    // o   o
      createSphere(union, new Vector3( 0.5,  0.5, -1.22)); // o o o
    }                                                      // o   X
  }

  function createSphere(parent, vec) {
    var sphere = new Sphere(parent);
    sphere.radius = dotSize;
    sphere.position = vec;
  }

  this.create = function() {
    var dice = new Difference(this);
    var box = new Box(dice);
    box.bevelRadius = 0.15;
    box.material = diceMaterial;

    var degrees = 0.01745329251996;

    createDots(dice, 1, new Vector3(180 * degrees, 0, 0));
    createDots(dice, 2, new Vector3(90 * degrees, 0, 0));
    createDots(dice, 3, new Vector3(0, 90 * degrees, 0));
    createDots(dice, 4, new Vector3(0, 270 * degrees, 0));
    createDots(dice, 5, new Vector3(270 * degrees, 0, 0));
    createDots(dice, 6, new Vector3(0, 0, 0));
  }
}
