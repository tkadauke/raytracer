# 7. Primitives and intersection

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

The primitive interface, then per-shape: sphere (analytic quadric),
plane, box (slab method), triangle (Möller-Trumbore), disk and open
cylinder, torus (quartic root finding). Each gets the math, the
hit-point construction, and the place where UVs get filled in.

## Source anchors

<!-- source-anchors -->
- `include/render/primitives/Primitive.h`
- `include/render/primitives/Sphere.h`
- `include/render/primitives/Plane.h`
- `include/render/primitives/Box.h`
- `include/render/primitives/Triangle.h`
- `include/render/primitives/Disk.h`
- `include/render/primitives/OpenCylinder.h`
- `include/render/primitives/Rectangle.h`
- `include/render/primitives/Torus.h`
- `include/render/primitives/MeshTriangle.h`
- `include/render/primitives/FlatMeshTriangle.h`
- `include/render/primitives/SmoothMeshTriangle.h`
- `include/core/math/Quartic.h`
- `include/core/math/Quadric.h`
- `include/core/math/Cubic.h`
- `include/core/math/Polynomial.h`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: mesh_triangle_interpolation -->
<!-- widget: sphere_farthest_point -->
<!-- widget: box_farthest_point -->
<!-- widget: convex_hull_farthest_point -->

Forward reference (full coverage in chapter 14):

<!-- widget: support_mapping_gjk -->

Plus per-primitive doc renders.

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [6. Cameras](06-cameras.md)
- Next: [8. Materials and BRDFs](08-materials-and-brdfs.md)
- Composed forms: [14. CSG](../03-scene-structure/14-csg.md)
- Mesh form for rasterization: [17. Tessellation](../04-rasterization/17-tessellation.md)
- Geometry vocabulary: [3. Rays and geometry](../01-foundations/03-rays-and-geometry.md)
