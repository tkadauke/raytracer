# glTF import

glTF 2.0 import is available through the shared scene importer registry, so
`rendercli`, Modeler open flows, and `SourceAsset` rebuilds use the same path.
Both `.gltf` JSON files and `.glb` binary containers are accepted.

```bash
tools/rendercli/rendercli --engine raster --width 640 --height 480 \
  test/fixtures/gltf/comprehensive_scene.gltf out.png
```

## Supported Data

The low-level reader resolves external files beside the imported asset, embedded
data URIs, and GLB BIN chunks. It validates buffers, buffer views, accessors,
images, scenes, nodes, cameras, animations, materials, textures, and mesh
references before the world importer runs.

The world importer currently preserves:

- scene and node hierarchy as `Group` objects, including local transforms,
  source IDs, and provenance metadata,
- triangle mesh primitives with `POSITION`, optional `NORMAL`, optional
  `TEXCOORD_0`, optional unsigned indices, and `TRIANGLES` mode,
- PBR metallic-roughness `baseColorFactor` as a matte diffuse color,
- PBR base-color image textures that use `TEXCOORD_0`,
- perspective and orthographic cameras attached to nodes,
- `KHR_lights_punctual` directional and point lights,
- simple node transform animation channels for translation, rotation, and scale.

Standalone glTF asset imports are wrapped in a product-view scene with a default
camera, directional light, dark background, and camera framing. If the glTF file
contains its own camera or light, those imported objects remain in the group
hierarchy as editable scene objects.

## Limits

The importer is intentionally a native subset, not a full DCC interchange stack.
Unsupported or downgraded data is reported as import diagnostics when the file
can still load.

- Mesh primitive modes other than `TRIANGLES` are skipped.
- Attributes beyond `POSITION`, `NORMAL`, and `TEXCOORD_0` are not imported.
- Materials are mapped to local matte shading. Metallic, roughness, emissive,
  occlusion, normal textures, clearcoat, transmission, volume, sheen, and
  specular extensions are not represented yet.
- Base-color textures using texture coordinates other than `TEXCOORD_0` are not
  sampled.
- Punctual spot lights are reported as unsupported.
- Skinning, morph targets, sparse accessors, Draco / meshopt compression,
  instancing extensions, and write support remain TODO.

## Fixtures And Smoke

[`test/fixtures/gltf/`](../../../test/fixtures/gltf/) contains small
hand-authored assets:

- `external_triangle.gltf` checks external buffer and image resolution,
- `animated_node.gltf` checks transform animation metadata and timeline import,
- `comprehensive_scene.gltf` checks hierarchy, mesh, material, texture, and
  camera import, and is rendered by `test/rendercli/ImportOptionTest.cmake`.

Unit tests pin the reader and world importer contracts. The rendercli smoke test
renders the comprehensive fixture through the raster engine and asserts that the
output image has the expected dimensions and varied pixels.

## Source anchors

<!-- source-anchors -->
- `include/core/formats/gltf/GltfAsset.h`
- `include/core/formats/gltf/GltfReader.h`
- `include/world/import/GltfSceneImporter.h`
- `src/core/formats/gltf/GltfReader.cpp`
- `src/world/import/GltfSceneImporter.cpp`
- `test/fixtures/gltf/`
- `test/rendercli/ImportOptionTest.cmake`
<!-- /source-anchors -->

## See also

- Previous: [Additive manufacturing import](additive-manufacturing-import.md)
- Next: [G-code parsing](gcode-parsing.md)
- [Tools & I/O index](README.md)
- [Top-level TOC](../README.md)
