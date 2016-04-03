var BoudingBoxClass = new Class({
  createCanvas: function() {
    var canvas = new Canvas(320, 240);
    
    // circle + bounding box
    canvas.add(new Circle(new Vector(3, -2), 1, "dashed"));
    canvas.add(new Rectangle(new Vector(2, -3), new Vector(2, 2)));
    
    // rect + bounding box
    var group = new Group();
    group.setTransform("translate(6, -5) rotate(45)");
    group.add(new Rectangle(new Vector(-1, -1), new Vector(2, 2), "dashed"));
    canvas.add(group);
    canvas.add(new Rectangle(new Vector(4.585, -6.414), new Vector(2.828, 2.828)));
    
    return canvas.toSVG();
  }
});

(function(scriptElement) {
  var figure = new BoudingBoxClass();
  
  scriptElement.parentNode.appendChild(figure.createCanvas());
})(document.scripts[document.scripts.length - 1]);
