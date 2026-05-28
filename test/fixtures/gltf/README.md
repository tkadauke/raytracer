# glTF fixtures

Tiny hand-authored glTF 2.0 files for importer tests.

- `external_triangle.gltf` exercises external buffer and image URI resolution.
- `animated_node.gltf` exercises node transform animation metadata and timeline conversion.
- `comprehensive_scene.gltf` is the render smoke fixture: hierarchy, triangle mesh,
  indices, normals, `TEXCOORD_0`, base-color material, image texture, and camera.

The fixtures intentionally avoid optional compression, skinning, morph targets, and
extension-heavy material graphs so they remain readable and stable in source control.
