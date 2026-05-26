// Native subset boolean fixture.
union() {
  cube([1.0, 0.4, 0.4], center = true);
  difference() {
    cube([0.4, 1.0, 0.4], center = true);
    translate([0.0, 0.0, 0.05])
      cube([0.2, 0.2, 0.6], center = true);
  }
}
