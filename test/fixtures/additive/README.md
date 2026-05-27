# Additive manufacturing fixtures

These fixtures are intentionally small, hand-authored files for importer and
rendercli smoke tests. They cover delivery-format geometry without depending on
Slicer, PrusaSlicer, Cura, or any slicer-specific project metadata.

- `wedge.stl` is ASCII STL with two triangular facets.
- `wedge.3mf` is an uncompressed 3MF package for the same wedge geometry.
- `two_layer_path.gcode` is a minimal two-layer toolpath with travel,
  extrusion, layer comments, feature comments, feed rates, and a temperature
  command.
