# Additive manufacturing import

`rendercli` can import small additive-manufacturing files directly through
the shared scene-importer registry. STL and 3MF become generated
`MeshPrimitive` scene geometry with a default camera, light, material, and
provenance metadata so the same file can be rendered headlessly without
wrapping it in a project scene.

The STL importer accepts ASCII STL and binary STL triangle streams. It treats
the file as delivery geometry: every facet becomes one flat-shaded triangle,
and no material, unit, repair, or manifold information is inferred. That keeps
the path useful for print previews and format smoke tests without pretending
STL is a rich scene format.

The 3MF importer targets the minimal geometry subset used by the repository's
fixtures: vertices and triangles from a model XML payload. It can read a raw
`.model` XML document saved with a `.3mf` extension and uncompressed 3MF
packages whose local file entries are stored, not deflated. The implementation
does not resolve materials, build transforms, components, production metadata,
relationships, thumbnails, textures, beam lattices, slice stacks, or compressed
ZIP entries.

G-code is handled separately because it is not mesh geometry. The G-code
importer visualizes toolpath segments as curves and exposes print-oriented
coloring and layer filters through `rendercli`.

## Source anchors

<!-- source-anchors -->
- `include/world/import/AdditiveManufacturingSceneImporter.h`
- `src/world/import/AdditiveManufacturingSceneImporter.cpp`
- `test/fixtures/additive/`
- `test/rendercli/ImportOptionTest.cmake`
<!-- /source-anchors -->
