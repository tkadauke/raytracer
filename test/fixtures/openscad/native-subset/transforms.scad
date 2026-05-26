// Native subset transform fixture.
union() {
  translate([-0.45, 0.0, 0.0])
    cube([0.35, 0.35, 0.35], center = true);
  translate([0.35, 0.0, 0.0])
    rotate([0, 0, 45])
      scale([1.4, 0.6, 1.0])
        cube([0.35, 0.35, 0.35], center = true);
}
