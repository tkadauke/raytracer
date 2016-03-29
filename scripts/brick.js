var width = 2
var height = 3
var length = 4
var color = null

var _propTypes = {
  'width': 'int',
  'height': 'int',
  'length': 'int',
  'color': 'Material'
}

var ldu = 1.0/20.0

function max(a, b) {
  return a < b ? b : a
}

function setWidth(value) {
  this.width = max(1, value)
}

function setHeight(value) {
  this.height = max(1, value)
}

function setLength(value) {
  this.length = max(1, value)
}

function create() {
  var frame = new Difference(this)
  frame.material = color
  var box = new Box(frame)
  box.size = new Vector3(10 * width * ldu, 4 * height * ldu, 10 * length * ldu)
  // box.bevelRadius = 0.5 * ldu
  var inside = new Box(frame)
  inside.size = new Vector3((10 * width - 4) * ldu, 4 * height * ldu, (10 * length - 4) * ldu)
  inside.position = new Vector3(0, 4 * ldu, 0)
  
  var cornerStudX = (-10 * width + 10) * ldu
  var studY = (-4 * height - 1) * ldu
  var cornerStudZ = (-10 * length + 10) * ldu
  
  for (var x = 0; x != width; x++) {
    for (var z = 0; z != length; z++) {
      var stud = new Cylinder(this)
      stud.position = new Vector3(cornerStudX + x * 20 * ldu, studY, cornerStudZ + z * 20 * ldu)
      stud.height = 6 * ldu
      stud.radius = 6 * ldu
      stud.material = color
      stud.bevelRadius = 0.5 * ldu
    }
  }
  
  if (width != 1 || length != 1) {
    // Small bars
    if (width == 1) {
      var cornerBarZ = (-10 * length + 20) * ldu
      for (var z = 0; z != length - 1; z++) {
        var bar = new Cylinder(this)
        bar.position = new Vector3(0, 0, cornerBarZ + z * 20 * ldu)
        bar.height = 8 * height * ldu
        bar.radius = 4 * ldu
        bar.material = color
      }
    } else if (length == 1) {
      var cornerBarX = (-10 * width + 20) * ldu
      for (var x = 0; x != width - 1; x++) {
        var bar = new Cylinder(this)
        bar.position = new Vector3(cornerBarX + x * 20 * ldu, 0, 0)
        bar.height = 8 * height * ldu
        bar.radius = 4 * ldu
        bar.material = color
      }
    } else {
      // Large tubes
      var cornerTubeX = (-10 * width + 20) * ldu
      var cornerTubeZ = (-10 * length + 20) * ldu
  
      for (var x = 0; x != width - 1; x++) {
        for (var z = 0; z != length - 1; z++) {
          var tube = new Ring(this)
          tube.position = new Vector3(cornerTubeX + x * 20 * ldu, 0, cornerTubeZ + z * 20 * ldu)
          tube.height = 8 * height * ldu
          tube.innerRadius = 6 * ldu
          tube.outerRadius = 8 * ldu
          tube.material = color
        }
      }
    }
  }
}

