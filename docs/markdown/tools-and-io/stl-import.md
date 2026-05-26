# STL import

STL is the smallest useful manufacturing mesh format: a file is only a list of
triangles. The importer supports both ASCII and binary STL and converts every
facet into a triangle in [`Mesh`](../../../include/core/geometry/Mesh.h), then
wraps that mesh in [`render::MeshPrimitive`](../../../include/render/primitives/MeshPrimitive.h)
for the renderer.

Because STL carries no units, material slots, UVs, hierarchy, or object names,
[`StlSceneImporter`](../../../include/world/import/StlSceneImporter.h) reports
those assumptions as diagnostics. Coordinates are treated as scene units, and
the generated primitive uses the default material. Facet normals are normalized
when present; if a facet normal is zero, the parser computes a normal from the
triangle's vertex winding so the result is deterministic.

Binary STL has one extra validation step: the parser checks that the 32-bit
triangle count in the file header exactly matches the remaining byte count.
That catches truncated files before a partial mesh can enter the scene.

## Source anchors

<!-- source-anchors -->
- `include/core/formats/stl/StlFile.h`
- `include/core/formats/stl/StlParseError.h`
- `include/world/import/StlSceneImporter.h`
- `src/core/formats/stl/StlFile.cpp`
- `src/world/import/StlSceneImporter.cpp`
- `test/fixtures/stl/`
- `test/unit/core/formats/stl/StlFileTest.cpp`
- `test/unit/world/import/StlSceneImporterTest.cpp`
<!-- /source-anchors -->

## See also

- Previous: [LDraw import](ldraw-import.md)
- Next: [Importer lifecycle](importer-lifecycle.md)
- [Tools & I/O index](README.md)
- [Top-level TOC](../README.md)
