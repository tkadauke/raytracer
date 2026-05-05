var style = document.createElement('style');
style.type = 'text/css';
style.innerHTML = `
svg * {
  stroke-width: 0.033;
}

svg .dashed {
  stroke-dasharray: 0.1, 0.1;
}

svg .red {
  stroke: #ff0000;
}

svg .red marker {
  stroke: #ff0000;
}

text {
  font-size: 3.3%;
}

line {
  stroke: #000000;
}

line.arrow {
  marker-end: url(#arrow);
}

line.axis {
  stroke-width: 0.05;
  marker-end: url(#arrow);
}

circle {
  stroke: #000000;
  fill: transparent;
}

circle.intersection {
  stroke: #000000;
  fill: #000000;
}

circle.result {
  stroke: #ff0000;
  fill: #ff0000;
}

rect {
  stroke: #000000;
  fill: transparent;
}
`;
document.getElementsByTagName('head')[0].appendChild(style);

var Class = function() {
  var parent = null, properties = {};
  if (typeof(arguments[0]) == "function") {
    parent = arguments[0];
    properties = arguments[1];
  } else {
    properties = arguments[0];
  }
  
  var klass = function() {
    this.initialize.apply(this, arguments);
  };
  
  if (parent) {
    var subclass = new Function;
    subclass.prototype = parent.prototype;
    klass.prototype = new subclass;
  }

  for (var property in properties) { 
    klass.prototype[property] = properties[property];
  }
      
  if (!klass.prototype.initialize)
    klass.prototype.initialize = function(){};

  return klass;
};

var OrderedHash = Class({
  initialize: function() {
    this._keys = [];
    this.vals = {};
  },
  
  push: function(k,v) {
    if (!this.vals[k])
      this._keys.push(k);
    this.vals[k] = v;
  },
  
  insert: function(pos,k,v) {
    if (!this.vals[k]) {
      this._keys.splice(pos,0,k);
      this.vals[k] = v;
    }
  },
  
  get: function(k) {
    return this.vals[k];
  },
  
  length: function() {
    return this._keys.length;
  },
  
  keys: function() {
    return this._keys;
  },
  
  sortedKeys: function() {
    return this.keys().sort(function(a,b) { return a - b;});
  },
  
  values: function(){
    return this.vals;
  }
});

var Vector = new Class({
  initialize: function(x, y) {
    this.x = x;
    this.y = y;
  },
  
  length: function() {
    return Math.sqrt(this.x * this.x + this.y * this.y);
  },
  
  plus: function(vector) {
    return new Vector(this.x + vector.x, this.y + vector.y);
  },
  
  minus: function(vector) {
    return new Vector(this.x - vector.x, this.y - vector.y);
  },
  
  multiply: function(scalar) {
    return new Vector(this.x * scalar, this.y * scalar);
  },
  
  dot: function(vector) {
    return this.x * vector.x + this.y * vector.y;
  },
  
  normalized: function() {
    return this.multiply(1.0 / this.length());
  },
  
  rotated: function(angle) {
    return new Vector(
      this.x * Math.cos(angle) - this.y * Math.sin(angle),
      this.x * Math.sin(angle) + this.y * Math.cos(angle)
    );
  }
});

Vector.null = new Vector(0, 0);
Vector.up = new Vector(0, -1);
Vector.right = new Vector(1, 0);

var svgns = "http://www.w3.org/2000/svg";

var Canvas = new Class({
  initialize: function(width, height) {
    this.width = width;
    this.height = height;
    this.elements = [];
    this.transform = "translate(0, " + height + ") scale(30, 30)";
  },
  
  add: function(element) {
    this.elements.push(element);
  },
  
  setTransform: function(transform) {
    this.transform = transform;
  },
  
  translate: function(vector) {
    this.transform += " translate(" + vector.x + ", " + vector.y + ")";
  },
  
  center: function() {
    this.translate(new Vector(5.5, -4));
  },
  
  toSVG: function() {
    var element = document.createElementNS(svgns, "svg");
    element.setAttribute("width", this.width);
    element.setAttribute("height", this.height);
    
    var defs = document.createElementNS(svgns, "defs");
    defs.innerHTML = `
    <marker id="arrow" markerWidth="10" markerHeight="10" refx="8" refy="3" orient="auto" markerUnits="strokeWidth">
      <path d="M0,0 L0,6 L9,3 z" fill="#000" />
    </marker>
    `;
    element.appendChild(defs);
    
    var group = document.createElementNS(svgns, "g");
    group.setAttribute("transform", this.transform);
    
    this.elements.forEach(function(e) {
      group.appendChild(e.toSVG());
    });
    
    element.appendChild(group);
    return element;
  }
});

var Group = new Class({
  initialize: function() {
    this.elements = [];
  },
  
  add: function(element) {
    this.elements.push(element);
  },
  
  setTransform: function(transform) {
    this.transform = transform;
  },
  
  toSVG: function() {
    var group = document.createElementNS(svgns, "g");
    group.setAttribute("transform", this.transform);
    
    this.elements.forEach(function(e) {
      group.appendChild(e.toSVG());
    });
    
    return group;
  }
});

var Line = new Class({
  initialize: function(origin, direction, klass) {
    this.origin = origin
    this.direction = direction
    this.klass = klass;
  },
  
  toSVG: function() {
    var line = document.createElementNS(svgns, "line");
    var end = this.origin.plus(this.direction);
    line.setAttribute("x1", this.origin.x);
    line.setAttribute("y1", this.origin.y);
    line.setAttribute("x2", end.x);
    line.setAttribute("y2", end.y);
    line.setAttribute("class", this.klass);
    return line;
  }
});

var Ray = new Class({
  initialize: function(origin, direction, both) {
    this.origin = origin
    this.direction = direction
    this.both = both;
  },
  
  toSVG: function() {
    var group = new Group();
    group.add(new Line(this.origin, this.direction, "arrow"));
    if (this.both) {
      group.add(new Line(this.at(-50), this.direction.multiply(100)));
    } else {
      group.add(new Line(this.origin, this.direction.multiply(100)));
    }
    return group.toSVG();
  },
  
  at: function(distance) {
    return this.origin.plus(this.direction.multiply(distance));
  },
  
  projectedDistance: function(vector) {
    return this.direction.dot(vector.minus(this.origin)) / this.direction.dot(this.direction);
  },
  
  projected: function(vector) {
    return this.at(this.projectedDistance(vector));
  }
});

var Circle = new Class({
  initialize: function(center, radius, klass) {
    this.center = center;
    this.radius = radius;
    this.klass = klass;
  },
  
  toSVG: function() {
    var circle = document.createElementNS(svgns, "circle");
    circle.setAttribute("cx", this.center.x);
    circle.setAttribute("cy", this.center.y);
    circle.setAttribute("r", this.radius);
    circle.setAttribute("class", this.klass);
    return circle;
  }
});

var Rectangle = new Class({
  initialize: function(topleft, size, klass) {
    this.topleft = topleft;
    this.size = size;
    this.klass = klass;
  },
  
  toSVG: function() {
    var rectangle = document.createElementNS(svgns, "rect");
    rectangle.setAttribute("x", this.topleft.x);
    rectangle.setAttribute("y", this.topleft.y);
    rectangle.setAttribute("width", this.size.x);
    rectangle.setAttribute("height", this.size.y);
    rectangle.setAttribute("class", this.klass);
    return rectangle;
  }
});

var Text = new Class({
  initialize: function(position, text, klass) {
    this.position = position;
    this.text = text;
    this.class = klass;
  },
  
  toSVG: function() {
    var text = document.createElementNS(svgns, "text");
    text.setAttribute("x", this.position.x);
    text.setAttribute("y", this.position.y);
    text.setAttribute("class", this.klass);
    text.innerHTML = this.text;
    return text;
  }
});

var Axes = new Class({
  initialize: function(length) {
    this.origin = Vector.null;
    this.length = length || 3;
  },
  
  toSVG: function() {
    var group = new Group();
    group.add(new Line(this.origin, new Vector(this.length, 0), "axis"));
    group.add(new Line(this.origin, new Vector(0, -this.length), "axis"));
    group.add(new Text(new Vector(this.length, 0.4), "x"));
    group.add(new Text(new Vector(-0.4, -this.length), "y"));
    return group.toSVG();
  }
});

// Arbitrary SVG path. Pass any valid SVG `d` attribute string —
// straight lines (M/L), curves (C/Q/A), polygons, anything path
// syntax supports. For simple shapes (line, circle, rect) use the
// dedicated primitives; reach for Path when you need a curve, a
// piecewise polygon, or a function plot.
//
// Example:
//   canvas.add(new Path("M 0 0 Q 1 1 2 0", "result"));   // quadratic
//   canvas.add(new Path("M 0 0 L 1 0 L 1 1 L 0 1 Z"));    // closed quad
//
// MDN's "SVG path" reference is the canonical syntax doc.
var Path = new Class({
  initialize: function(d, klass) {
    this.d = d;
    this.klass = klass;
  },

  toSVG: function() {
    var path = document.createElementNS(svgns, "path");
    path.setAttribute("d", this.d);
    if (this.klass) path.setAttribute("class", this.klass);
    path.setAttribute("fill", "transparent");
    return path;
  }
});

// Helper: build a Path "d" string from an array of {x, y} points,
// connecting them with straight line segments. The first point is
// the move-to; the rest are line-to. Pass `closed: true` to close
// the polygon (Z command at the end).
//
// Example:
//   var d = Path.polyline([new Vector(0, 0), new Vector(1, 0),
//                          new Vector(1, 1)], { closed: true });
//   canvas.add(new Path(d));
Path.polyline = function(points, opts) {
  opts = opts || {};
  if (points.length === 0) return "";
  var d = "M " + points[0].x + " " + points[0].y;
  for (var i = 1; i < points.length; i++) {
    d += " L " + points[i].x + " " + points[i].y;
  }
  if (opts.closed) d += " Z";
  return d;
};

// HTML range slider with a live label. Use this instead of (or
// alongside) DragHandler for widgets where the user benefits from
// seeing the parameter value, having a defined range, and being
// able to grab a known affordance.
//
// Example:
//   var slider = new Slider({
//     label: "focalDistance", min: 1, max: 7, value: 4,
//     onChange: function(v) {
//       figure.focalDistance = v;
//       redraw();
//     }
//   });
//   container.appendChild(slider.element());
//
// Returns a div containing the label + slider so callers can
// position it anywhere (above the SVG canvas is the conventional
// placement).
var Slider = new Class({
  initialize: function(opts) {
    this.label = opts.label || "";
    this.min = opts.min;
    this.max = opts.max;
    this.step = opts.step || (this.max - this.min) / 100.0;
    this.value = opts.value !== undefined ? opts.value : (this.min + this.max) / 2.0;
    this.precision = opts.precision !== undefined ? opts.precision : 2;
    this.onChange = opts.onChange || function () {};
  },

  element: function() {
    var div = document.createElement("div");
    div.style.fontFamily = "sans-serif";
    div.style.fontSize = "13px";
    div.style.padding = "4px 0";
    div.style.display = "flex";
    div.style.alignItems = "center";
    div.style.gap = "8px";

    var label = document.createElement("label");
    var format = function(v, precision) {
      return v.toFixed(precision);
    };
    label.textContent = this.label + " = " + format(this.value, this.precision);
    label.style.minWidth = "12em";

    var input = document.createElement("input");
    input.type = "range";
    input.min = this.min;
    input.max = this.max;
    input.step = this.step;
    input.value = this.value;
    input.style.flex = "1";

    var precision = this.precision;
    var labelText = this.label;
    var onChange = this.onChange;
    input.addEventListener("input", function (e) {
      var v = parseFloat(e.target.value);
      label.textContent = labelText + " = " + format(v, precision);
      onChange(v);
    });

    div.appendChild(label);
    div.appendChild(input);
    return div;
  }
});

var DragHandler = new Class({
  initialize: function(figure) {
    this.handlerFunc = function() {}
    this.figure = figure;
  },
  
  divElement: function() {
    if (this.element) {
      return this.element;
    }
    
    this.element = document.createElement("div");
    
    var mousex = null;
    var mousey = null;
  
    this.element.addEventListener("mousedown", function(e) {
      mousex = e.pageX;
      mousey = e.pageY;
      
      document.onmousemove = function(event) {
        event = event || window.event;
        
        if (this.handlerFunc(new Vector(event.pageX - mousex, event.pageY - mousey), this.figure)) {
          var newCanvas = this.figure.createCanvas();
          this.element.replaceChild(newCanvas, this.canvas);
          this.canvas = newCanvas;
          event.stopPropagation();
          event.preventDefault();
        }
        
        mousex = event.pageX;
        mousey = event.pageY;
      }.bind(this);
      
      document.onmouseup = function() {
        document.onmousemove = null;
        mousex = null;
        mousey = null;
      }.bind(this);
      
      e.stopPropagation();
      e.preventDefault();
    }.bind(this));
    
    this.canvas = this.figure.createCanvas();
    this.canvas.onselectstart = function(){return false};
    this.canvas.unselectable = "on";
    
    this.element.appendChild(this.canvas);
    this.element.unselectable = "on";
    this.element.onselectstart = function(){return false};
    this.element.style.userSelect = "none";
    
    return this.element;
  }
});

var degrees = 0.01745329251996;
