var diceMaterial = null;
var dotMaterial = null;
var dotSize = 0.3;

var _propTypes = {
  'diceMaterial': 'Material',
  'dotMaterial': 'Material',
  'dotSize': 'double'
}

var degrees = 0.01745329251996;

function create() {
  var dice = new Difference(this);
  var box = new Box(dice);
  box.bevelRadius = 0.15;
  box.material = diceMaterial;
  
  createDots(dice, 1, new Vector3(180 * degrees, 0, 0));
  createDots(dice, 2, new Vector3(90 * degrees, 0, 0));
  createDots(dice, 3, new Vector3(0, 90 * degrees, 0));
  createDots(dice, 4, new Vector3(0, 270 * degrees, 0));
  createDots(dice, 5, new Vector3(270 * degrees, 0, 0));
  createDots(dice, 6, new Vector3(0, 0, 0));
}

function createDots(parent, num, rotation) {
  var union = new Union(parent);
  union.name = "Face " + num;
  union.material = dotMaterial;
  union.rotation = rotation;
  
  if (num == 4 || num == 5 || num == 6) {
    createSphere(union, new Vector3(-0.5, -0.5, -1.22));
  }

  if (num == 6) {
    createSphere(union, new Vector3(-0.5,  0.0, -1.22));
  }

  if (num == 2 || num == 3 || num == 4 || num == 5 || num == 6) {
    createSphere(union, new Vector3(-0.5,  0.5, -1.22));
  }

  if (num == 1 || num == 3 || num == 5) {
    createSphere(union, new Vector3( 0.0,  0.0, -1.22));
  }

  if (num == 2 || num == 3 || num == 4 || num == 5 || num == 6) {
    createSphere(union, new Vector3( 0.5, -0.5, -1.22));
  }

  if (num == 6) {
    createSphere(union, new Vector3( 0.5,  0.0, -1.22));
  }

  if (num == 4 || num == 5 || num == 6) {
    createSphere(union, new Vector3( 0.5,  0.5, -1.22));
  }
}

function createSphere(parent, vec) {
  var sphere = new Sphere(parent);
  sphere.radius = dotSize;
  sphere.position = vec;
}
