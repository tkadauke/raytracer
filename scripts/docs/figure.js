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
