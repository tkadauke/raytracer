# OpenSCAD import

OpenSCAD support is source-asset import, not an embedded CAD kernel. The
currently available path invokes an external `openscad` executable, compiles a
`.scad` source file into STL or PLY, imports the generated mesh, and caches that
mesh by source contents plus import options.

## External compiler path

Install OpenSCAD and make the `openscad` executable available on `PATH`, or pass
the executable path through import options. A `SourceAsset` can pin the same
options in scene JSON:

```json
{
  "type": "SourceAsset",
  "sourcePath": "part.scad",
  "format": "openscad",
  "importOptions": {
    "executable": "/usr/bin/openscad",
    "cacheDirectory": "build/openscad-cache",
    "outputFormat": "stl"
  }
}
```

`outputFormat` accepts `stl` and `ply`. If `executable` is empty, the importer
searches `PATH`. If no executable is found, the import reports a warning and
returns an empty group so the rest of the scene can still load. A compiler
failure, unsupported generated mesh format, or malformed generated mesh is an
import error.

`cacheDirectory` is optional. When it is omitted, the importer writes generated
meshes under the platform cache location. Cache keys include the source file
contents and import options, so changing `define`, `outputFormat`, or the source
file causes a fresh compile.

## Native subset fixtures

The repository also keeps native-subset fixture sources under
[`test/fixtures/openscad/native-subset/`](../../../test/fixtures/openscad/native-subset/).
They define the import surface expected of a future in-process OpenSCAD subset:

- simple primitives: `cube` and `sphere`;
- transforms: `translate`, `rotate`, and `scale`;
- booleans: `union` and `difference`.

Those fixtures intentionally avoid modules, variables, loops, list
comprehensions, projection, text, surface imports, and full OpenSCAD expression
evaluation. Until a native importer is registered, rendercli smoke coverage uses
the external compiler path when it is available.

## Smoke coverage

`test/rendercli/ImportOptionTest.cmake` always renders the OpenSCAD SourceAsset
path with a fake compiler that emits deterministic STL output. It also attempts
to locate a real `openscad` executable and renders
[`test/fixtures/openscad/external-compiler/compiler_smoke.scad`](../../../test/fixtures/openscad/external-compiler/compiler_smoke.scad)
when the tool is installed; otherwise that smoke is reported as skipped by the
CMake test script.

## Source anchors

<!-- source-anchors -->
- `include/world/import/OpenScadCompiler.h`
- `include/world/import/OpenScadSceneImporter.h`
- `src/world/import/OpenScadCompiler.cpp`
- `src/world/import/OpenScadSceneImporter.cpp`
- `test/fixtures/openscad/`
- `test/rendercli/ImportOptionTest.cmake`
- `test/unit/world/import/OpenScadSceneImporterTest.cpp`
<!-- /source-anchors -->

## See also

- Previous: [LDraw import](ldraw-import.md)
- Next: [Importer lifecycle](importer-lifecycle.md)
- [Tools & I/O index](README.md)
- [Top-level TOC](../README.md)
