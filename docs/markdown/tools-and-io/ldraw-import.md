# LDraw import

LDraw support is meant to make small part libraries and model assemblies
renderable without requiring the full official library during development. The
core parser accepts `.dat`, `.ldr`, and `.mpd` text files, and `rendercli` can
either load LDraw directly or expand LDraw authoring metadata embedded in a
world scene JSON file.

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

The library root should be the directory that contains standard LDraw
subdirectories such as `parts/`, `parts/s/`, `p/`, `p/48/`, and `models/`.
The importer searches the model's own directory first, then those library
subdirectories. That makes a project-local mini-library enough for validation:
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
- `include/world/import/LDrawSceneImporter.h`
- `src/core/formats/ldraw/LDrawParser.cpp`
- `src/core/formats/ldraw/LDrawColorTable.cpp`
- `src/core/formats/ldraw/LDrawFileResolver.cpp`
- `src/core/formats/ldraw/LDrawGeometryCompiler.cpp`
- `src/world/import/LDrawSceneImporter.cpp`
- `test/fixtures/ldraw/`
- `test/rendercli/RaytracerOptionTest.cmake`
- `test/unit/core/formats/ldraw/LDrawGeometryCompilerTest.cpp`
- `test/unit/world/objects/LDrawSceneImporterTest.cpp`
<!-- /source-anchors -->

## See also

- Previous: [PLY parsing](ply-parsing.md)
- Next: [Importer lifecycle](importer-lifecycle.md)
- [Tools & I/O index](README.md)
- [Top-level TOC](../README.md)
