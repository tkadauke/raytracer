# G-code parsing

G-code is the line-oriented toolpath format produced by 3D-printer
slicers. For visualization, the useful subset is not every firmware
command; it is the motion path, extrusion amount, feed rate, layer
markers, and the printer metadata that explains what a segment means.

[`GCodeParser`](../../../include/core/formats/gcode/GCodeParser.h)
keeps that subset in a format-neutral program model. `G0` and `G1`
become motion segments with start/end points, feed rate, extrusion
delta, current layer index, and slicer feature type. `G90` / `G91`
switch absolute and relative XYZ positioning, while `M82` / `M83`
switch absolute and relative extrusion. `G92` updates the current
position or extruder origin without emitting a visible segment, which
is how slicers commonly reset the E axis.

Comments are preserved, and common slicer comments such as `;LAYER:`,
`;LAYER_CHANGE`, `;Z:`, and `;TYPE:` are promoted to structured
metadata. Temperature and tool commands (`M104`, `M109`, `M140`,
`M190`, `T0`, `T1`, ...) are stored separately from movement so a
future importer can display setup or heating events without treating
them as geometry. Unknown and unsupported commands are ignored with
diagnostics instead of aborting the parse; printer dialects routinely
add firmware-specific commands that are irrelevant to a path preview.

The parser intentionally stops at the core format layer. Turning the
program into `Group` layers and `core::Polyline` / `render::Curve`
objects is the next importer step.

## Source anchors

<!-- source-anchors -->
- `include/core/formats/gcode/GCodeDiagnostic.h`
- `include/core/formats/gcode/GCodeParser.h`
- `include/core/formats/gcode/GCodeProgram.h`
- `src/core/formats/gcode/GCodeParser.cpp`
- `test/unit/core/formats/gcode/GCodeParserTest.cpp`
- `test/fixtures/gcode/`
<!-- /source-anchors -->
