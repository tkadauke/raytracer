# 17. Tessellation

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

Why a rasterizer needs triangles where the raytracer needs implicit
surfaces, the `tessellate(int lod) → Mesh` contract, the
per-primitive strategies (sphere → subdivided icosahedron / lat-long
grid, disk → triangle fan, cylinder → quad strip with
seam-duplicate UVs, etc.), how `Composite` and `Instance` recurse.

## Source anchors

<!-- source-anchors -->
- `include/core/geometry/Mesh.h`
- `include/render/primitives/Primitive.h`
- `include/render/primitives/Sphere.h`
- `include/render/primitives/Disk.h`
- `include/render/primitives/OpenCylinder.h`
- `include/render/primitives/Torus.h`
- `include/render/primitives/Box.h`
- `include/render/primitives/Triangle.h`
- `include/render/primitives/Rectangle.h`
- `include/render/primitives/Composite.h`
- `include/render/primitives/Instance.h`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: sphere_tessellate -->
<!-- widget: disk_tessellate -->
<!-- widget: open_cylinder_tessellate -->
<!-- widget: torus_tessellate -->

Plus per-primitive tessellation doc renders at multiple LODs.

## See also

- Volume index: [Volume IV — Rasterization](README.md)
- Previous: [16. Instances and motion blur](../03-scene-structure/16-instances-and-motion-blur.md)
- Next: [18. The rasterization pipeline](18-the-rasterization-pipeline.md)
- Implicit-surface side: [7. Primitives and intersection](../02-ray-rendering/07-primitives-and-intersection.md)
