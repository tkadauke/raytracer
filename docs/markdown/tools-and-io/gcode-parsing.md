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

`GCodePathCompiler` turns the parsed motion stream into visible path
curves. Non-zero travel and extrusion moves become distinct
`core::Polyline` / `render::Curve` categories with per-segment
attributes for `move_type`, `speed` / `feed_rate`, extrusion amount,
tool, layer, feature type, and source line. The world importer wraps
those curves in `Group` hierarchy: a top-level G-code import group,
layer groups with step/layer metadata, then tool and slicer feature
groups where that metadata is available.

The supported subset is intentionally visualization-oriented: linear
movement (`G0` / `G1`), absolute/relative XYZ modes, absolute/relative
extrusion modes, extruder resets, feed rates, temperature commands, tool
changes, and common layer/feature comments. The importer does not simulate
printer firmware, arcs, bed leveling, acceleration, pressure advance,
volumetric extrusion, retraction semantics beyond the E delta, macros, or
machine-specific dialect commands. Unsupported commands produce diagnostics
and are ignored when they do not affect the visible path preview.

## Source anchors

<!-- source-anchors -->
- `include/core/formats/gcode/GCodeDiagnostic.h`
- `include/core/formats/gcode/GCodePathCompiler.h`
- `include/core/formats/gcode/GCodeParser.h`
- `include/core/formats/gcode/GCodeProgram.h`
- `include/world/import/GCodeSceneImporter.h`
- `src/core/formats/gcode/GCodePathCompiler.cpp`
- `src/core/formats/gcode/GCodeParser.cpp`
- `src/world/import/GCodeSceneImporter.cpp`
- `test/unit/core/formats/gcode/GCodePathCompilerTest.cpp`
- `test/unit/core/formats/gcode/GCodeParserTest.cpp`
- `test/unit/world/import/GCodeSceneImporterTest.cpp`
- `test/fixtures/gcode/`
<!-- /source-anchors -->
