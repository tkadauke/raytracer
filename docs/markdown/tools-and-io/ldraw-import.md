# LDraw import

LDraw support is meant to make small part libraries and model assemblies
renderable without requiring the full official library during development. The
core parser accepts `.dat`, `.ldr`, and `.mpd` text files. `rendercli` and the
Modeler can open those files through the shared scene importer, and scene JSON
can also expand LDraw authoring metadata embedded in a world scene file.

The supported visible subset is deliberately mesh-focused:

- line type 0 meta commands are preserved, with `FILE` / `NOFILE` used for MPD
  submodels and `BFC` used for winding, sidedness, and `INVERTNEXT`;
- line type 1 subfile references resolve through the current model directory
  and configured library roots, apply affine transforms, and inherit color 16 /
  edge color 24 context;
- line types 3 and 4 become lit mesh geometry;
- line type 2 edge/detail lines become wireframe curve-overlay segments;
- `LDConfig.ldr` `!COLOUR` records, direct RGB color codes, transparent colors,
  and common finish hints map to renderer materials.

Type-5 conditional lines are parsed but skipped with diagnostics. Texture
mapping meta commands, unofficial custom geometry extensions, and full
LDraw-specific material fidelity are not implemented; unsupported records should
produce diagnostics instead of silently changing the model.

## Library paths

For direct LDraw input, pass the model file as the input scene and add
`--ldraw_input`:

```sh
tools/rendercli/rendercli --ldraw_input \
  --ldraw_library_root /path/to/ldraw \
  model.mpd out.png
```

Direct LDraw input applies `ldraw_to_raytracer` coordinate conversion by
default so standard LDraw Y-down models arrive in the renderer's Y-up scene
space. Use `--ldraw_coordinate_conversion none` for hand-authored fixtures that
already use renderer coordinates.

Direct LDraw input also frames the generated scene's pinhole camera around the
compiled model bounds from a three-quarter view. The fit calculation is shared
by `Scene` and `PinholeCamera`, so UI import paths can reuse the same camera
placement instead of carrying a rendercli-only heuristic.

`rendercli` can also import LDraw by filename extension through the generic
scene importer path:

```sh
tools/rendercli/rendercli --ldraw_library_root /path/to/ldraw \
  model.mpd out.png
```

The Modeler Open dialog accepts `.ldr`, `.dat`, and `.mpd` files. Opening one
imports the model into a new editable scene shell in the background, adds a
default directional light, and frames the scene camera around the compiled model
bounds. Imported scenes are not treated as save targets for the source
`.mpd`/`.ldr`; saving prompts for a normal scene JSON file.

The library root should be the directory that contains standard LDraw
subdirectories such as `parts/`, `parts/s/`, `p/`, `p/48/`, and `models/`.
The importer searches the model's own directory first, then the configured
library root. Modeler uses the importer's defaults, which auto-detect
`LDRAWDIR` and then `~/Documents/ldraw` before falling back to just the model's
own directory. That makes a project-local mini-library enough for validation:
[`test/fixtures/ldraw/smoke/`](../../../test/fixtures/ldraw/smoke/) renders
through CTest without the official parts library installed.

World scene JSON can also keep an editable `Collection` with metadata:

```json
{
  "type": "Collection",
  "metadata": {
    "sourceFormat": "LDraw",
    "sourcePath": "models/demo.mpd",
    "libraryPath": "ldraw",
    "normalMode": "flat"
  }
}
```

`rendercli --ldraw_library_root` overrides metadata `libraryPath`, which is
useful for CI and for moving a scene between machines. `normalMode` accepts
`flat` or `smooth`; flat normals preserve the hard-edged look of most LDraw
parts.

## Fixtures and smoke tests

The checked-in fixtures under
[`test/fixtures/ldraw/`](../../../test/fixtures/ldraw/) are hand-authored and
small enough to audit:

- `smoke/model.mpd` covers inline geometry, MPD-local submodels, library part
  resolution, nested subfiles, color inheritance, BFC winding, and edge-line
  color inheritance;
- `nested/` covers recursive transform composition;
- `repeated/` covers compiled part cache reuse;
- `rendercli/` keeps a minimal one-part CLI option fixture.

`test/rendercli/RaytracerOptionTest.cmake` renders the smoke MPD through direct
LDraw input with the mini-library root, so the integration path is exercised
without external assets.

## Source anchors

<!-- source-anchors -->
- `include/core/formats/ldraw/LDrawParser.h`
- `include/core/formats/ldraw/LDrawCommand.h`
- `include/core/formats/ldraw/LDrawColorTable.h`
- `include/core/formats/ldraw/LDrawFileResolver.h`
- `include/core/formats/ldraw/LDrawGeometryCompiler.h`
- `include/world/import/LDrawFileSceneImporter.h`
- `include/world/import/LDrawSceneImporter.h`
- `include/world/objects/PinholeCamera.h`
- `include/world/objects/Scene.h`
- `src/core/formats/ldraw/LDrawParser.cpp`
- `src/core/formats/ldraw/LDrawCommand.cpp`
- `src/core/formats/ldraw/LDrawColorTable.cpp`
- `src/core/formats/ldraw/LDrawFileResolver.cpp`
- `src/core/formats/ldraw/LDrawGeometryCompiler.cpp`
- `src/modeler/MainWindow.cpp`
- `src/world/import/LDrawFileSceneImporter.cpp`
- `src/world/import/LDrawSceneImporter.cpp`
- `src/world/objects/PinholeCamera.cpp`
- `src/world/objects/Scene.cpp`
- `test/fixtures/ldraw/`
- `test/rendercli/RaytracerOptionTest.cmake`
- `test/unit/core/formats/ldraw/LDrawColorTableTest.cpp`
- `test/unit/core/formats/ldraw/LDrawFileResolverTest.cpp`
- `test/unit/core/formats/ldraw/LDrawGeometryCompilerTest.cpp`
- `test/unit/world/import/LDrawFileSceneImporterTest.cpp`
- `test/unit/world/objects/LDrawSceneImporterTest.cpp`
<!-- /source-anchors -->

## See also

- Previous: [PLY parsing](ply-parsing.md)
- Next: [Importer lifecycle](importer-lifecycle.md)
- [Tools & I/O index](README.md)
- [Top-level TOC](../README.md)
