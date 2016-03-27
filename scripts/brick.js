var width = 2
var height = 3
var length = 4
var color = null

var _propTypes = {
  'width': 'double',
  'height': 'double',
  'length': 'double',
  'color': 'Material'
}

var ldu = 1.0/20.0

function create() {
  var box = new Box(this)
  box.size = new Vector3(10 * width * ldu, 4 * height * ldu, 10 * length * ldu)
  box.material = color
  
  var cornerStudX = (-10 * width + 10) * ldu
  var studY = (-4 * height - 2) * ldu
  var cornerStudZ = (-10 * length + 10) * ldu
  
  for (var x = 0; x != width; x++) {
    for (var z = 0; z != length; z++) {
      var stud = new Cylinder(box)
      stud.position = new Vector3(cornerStudX + x * 20 * ldu, studY, cornerStudZ + z * 20 * ldu)
      stud.height = 4 * ldu
      stud.radius = 6 * ldu
      stud.material = color
    }
  }
}
