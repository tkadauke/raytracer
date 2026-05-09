# Appendix A — Glossary

> **Status:** Stub. Will fill in as terms appear in chapters. The
> entries below are the placeholders for ~50 terms eventually
> defined here. Each entry is one line and links to the chapter
> that introduces the concept.

Hand-written; *not* generated. Drift is fixed by editing this file.

## A

- **AABB** — Axis-Aligned Bounding Box. Defined in
  [chapter 3](../01-foundations/03-rays-and-geometry.md).
- **ACES** — Academy Color Encoding System. The Narkowicz fit
  ships as one of three tonemap operators in
  [chapter 12](../02-ray-rendering/12-tone-mapping.md).
- **AOV** — Arbitrary Output Variable. Future topic; not yet in
  the codebase.

## B

- **Barycentric coordinates** — defined in
  [chapter 7](../02-ray-rendering/07-primitives-and-intersection.md).
- **BRDF** — Bidirectional Reflectance Distribution Function.
  [Chapter 8](../02-ray-rendering/08-materials-and-brdfs.md).
- **BSDF** — Bidirectional Scattering Distribution Function.
  Container abstraction over BRDF + BTDF. [Chapter 8](../02-ray-rendering/08-materials-and-brdfs.md).
- **BTDF** — Bidirectional Transmittance Distribution Function.
  [Chapter 8](../02-ray-rendering/08-materials-and-brdfs.md).
- **BVH** — Bounding Volume Hierarchy.
  [Chapter 15](../03-scene-structure/15-spatial-acceleration.md).

## C

- **CSG** — Constructive Solid Geometry.
  [Chapter 14](../03-scene-structure/14-csg.md).

## D

- **DDA** — Digital Differential Analyzer. The grid traversal
  algorithm. [Chapter 15](../03-scene-structure/15-spatial-acceleration.md).
- **DOF** — Depth of Field. [Chapter 6](../02-ray-rendering/06-cameras.md).

## E

- **Edge function** — Pineda 1988 inside-test for triangle
  rasterization. [Chapter 18](../04-rasterization/18-the-rasterization-pipeline.md).

## G

- **GJK** — Gilbert-Johnson-Keerthi distance algorithm.
  [Chapter 14](../03-scene-structure/14-csg.md).

## H

- **HDR** — High Dynamic Range. Float-precision colors before
  tonemap. [Chapter 12](../02-ray-rendering/12-tone-mapping.md).
- **Homogeneous coordinates** — 4-vector representation of 3D
  points / directions. [Chapter 2](../01-foundations/02-matrices-and-transforms.md).

## L

- **LDR** — Low Dynamic Range. 8-bit-per-channel display values.
  [Chapter 12](../02-ray-rendering/12-tone-mapping.md).
- **LOD** — Level of Detail. The integer parameter that controls
  tessellation density. [Chapter 17](../04-rasterization/17-tessellation.md).

## M

- **Möller-Trumbore** — the canonical ray–triangle intersection
  algorithm. [Chapter 7](../02-ray-rendering/07-primitives-and-intersection.md).
- **MSAA** — Multi-Sample Anti-Aliasing. Coverage sampling without
  shading multiplier. [Chapter 21](../04-rasterization/21-msaa-and-attribute-interpolation.md).

## P

- **Phong** — the classical lobe-based reflection model.
  [Chapter 8](../02-ray-rendering/08-materials-and-brdfs.md).
- **Pinhole camera** — projection model with infinite depth of
  field. [Chapter 6](../02-ray-rendering/06-cameras.md).
- **PLY** — Polygon File Format. The codebase's mesh-import
  format. [Chapter 25](../06-tools-and-io/25-ply-parsing.md).

## R

- **Ray** — origin + direction + parameter `t`.
  [Chapter 3](../01-foundations/03-rays-and-geometry.md).

## S

- **SAH** — Surface Area Heuristic. BVH split-selection criterion.
  [Chapter 15](../03-scene-structure/15-spatial-acceleration.md).
- **Sutherland-Hodgman** — polygon-clipping algorithm against a
  half-space. [Chapter 19](../04-rasterization/19-clipping-depth-stencil.md).
- **SSE3** — Streaming SIMD Extensions 3. Used for the
  vector / color hot paths. [Chapter 1](../01-foundations/01-numbers-and-vectors.md).

## T

- **Tonemap** — function from HDR to LDR.
  [Chapter 12](../02-ray-rendering/12-tone-mapping.md).

## U

- **UV** — texture coordinates, in `(s, t)` ∈ [0, 1]² by
  convention. [Chapter 11](../02-ray-rendering/11-textures.md).

## W

- **Whitted** — the recursive ray-tracing algorithm (Whitted
  1980). [Chapter 5](../02-ray-rendering/05-the-whitted-pipeline.md).

## Z

- **Z-buffer** — per-pixel depth buffer for visibility resolution.
  [Chapter 19](../04-rasterization/19-clipping-depth-stencil.md).

---

## See also

- [Top-level TOC](../README.md)
- [B. Bibliography](b-bibliography.md)
- [C. Source map](c-source-map.md)
