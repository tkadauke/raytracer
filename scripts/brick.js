// Lego-style brick generator. `width`, `length`, `height` are integer
// stud counts; `color` is the material applied to every part.
//
// Properties are exposed on the host ScriptedSurface as editable
// fields via the `properties` map below; setX functions trigger a
// rebuild via the dynamic-property change event.
var properties = {
  width: 'int',
  height: 'int',
  length: 'int',
  color: 'Material'
}

function brick() {
  this.width = 2
  this.height = 3
  this.length = 4
  this.color = null

  // Lego unit — one ldu (Lego draw unit) is the canonical reference
  // size used by the LDraw library. Closure-scoped because it's
  // shared across every dimension calculation below.
  var ldu = 1.0/20.0

  function max(a, b) {
    return a < b ? b : a
  }

  this.setWidth = function(value) {
    this.width = max(1, value)
  }

  this.setHeight = function(value) {
    this.height = max(1, value)
  }

  this.setLength = function(value) {
    this.length = max(1, value)
  }

  this.create = function() {
    // QJSEngine (Qt 6) resolves bare names by JS scoping rules — bare
    // `width` / `color` etc. don't fall through to `this`. Pull the
    // current values into local variables once at the top so the
    // body stays readable.
    var w = this.width
    var h = this.height
    var l = this.length
    var color = this.color

    var union = new Union(this)
    var frame = new Difference(union)
    frame.material = color
    var box = new Box(frame)
    box.size = new Vector3(10 * w * ldu, 4 * h * ldu, 10 * l * ldu)
    // box.bevelRadius = 0.5 * ldu
    var inside = new Box(frame)
    inside.size = new Vector3((10 * w - 4) * ldu, 4 * h * ldu, (10 * l - 4) * ldu)
    inside.position = new Vector3(0, 4 * ldu, 0)

    var cornerStudX = (-10 * w + 10) * ldu
    var studY = (-4 * h - 1) * ldu
    var cornerStudZ = (-10 * l + 10) * ldu

    for (var x = 0; x != w; x++) {
      for (var z = 0; z != l; z++) {
        var stud = new Cylinder(union)
        stud.position = new Vector3(cornerStudX + x * 20 * ldu, studY, cornerStudZ + z * 20 * ldu)
        stud.height = 6 * ldu
        stud.radius = 6 * ldu
        stud.material = color
        stud.bevelRadius = 0.5 * ldu
      }
    }

    if (w != 1 || l != 1) {
      // Small bars
      if (w == 1) {
        var cornerBarZ = (-10 * l + 20) * ldu
        for (var z = 0; z != l - 1; z++) {
          var bar = new Cylinder(union)
          bar.position = new Vector3(0, 0, cornerBarZ + z * 20 * ldu)
          bar.height = 8 * h * ldu
          bar.radius = 4 * ldu
          bar.material = color
        }
      } else if (l == 1) {
        var cornerBarX = (-10 * w + 20) * ldu
        for (var x = 0; x != w - 1; x++) {
          var bar = new Cylinder(union)
          bar.position = new Vector3(cornerBarX + x * 20 * ldu, 0, 0)
          bar.height = 8 * h * ldu
          bar.radius = 4 * ldu
          bar.material = color
        }
      } else {
        // Large tubes
        var cornerTubeX = (-10 * w + 20) * ldu
        var cornerTubeZ = (-10 * l + 20) * ldu

        for (var x = 0; x != w - 1; x++) {
          for (var z = 0; z != l - 1; z++) {
            var tube = new Ring(union)
            tube.position = new Vector3(cornerTubeX + x * 20 * ldu, 0, cornerTubeZ + z * 20 * ldu)
            tube.height = 8 * h * ldu
            tube.innerRadius = 6 * ldu
            tube.outerRadius = 8 * ldu
            tube.material = color
          }
        }
      }
    }
  }
}
