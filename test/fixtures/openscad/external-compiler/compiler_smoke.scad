// External OpenSCAD compiler smoke fixture.
// Covers primitives, transforms, and booleans in one tiny printable solid.
difference() {
  union() {
    cube([1.2, 1.0, 0.5], center = true);
    translate([0.0, 0.0, 0.35])
      cylinder(h = 0.7, r = 0.32, center = true, $fn = 16);
  }

  translate([0.0, 0.0, 0.35])
    cylinder(h = 0.9, r = 0.12, center = true, $fn = 16);
}
