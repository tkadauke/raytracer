# Appendix B — Bibliography

> **Status:** Stub. Will fill in as chapters cite. The numbered
> entries below are the placeholders for the foundational
> citations the book draws on. Hand-written; *not* generated.

Each entry uses a stable `[N]` numeric key so chapter citations
look like *"the algorithm dates to [Pineda 1988](../appendix/b-bibliography.md#1)"*.

## Foundational rendering

1. <a id="1"></a>**Pineda, J.** (1988). "A Parallel Algorithm for
   Polygon Rasterization." *Proceedings of SIGGRAPH '88.*
   The edge-function rasterization algorithm used in
   [chapter 18](../04-rasterization/18-the-rasterization-pipeline.md).

2. <a id="2"></a>**Whitted, T.** (1980). "An Improved Illumination
   Model for Shaded Display." *Communications of the ACM, 23(6).*
   The recursive ray-tracing algorithm that names
   [chapter 5](../02-ray-rendering/05-the-whitted-pipeline.md).

3. <a id="3"></a>**Möller, T., Trumbore, B.** (1997). "Fast,
   Minimum Storage Ray-Triangle Intersection." *Journal of
   Graphics Tools, 2(1).* The triangle-intersection algorithm
   used in [chapter 7](../02-ray-rendering/07-primitives-and-intersection.md).

4. <a id="4"></a>**MacDonald, J. D., Booth, K. S.** (1990).
   "Heuristics for ray tracing using space subdivision." *The
   Visual Computer, 6.* The Surface Area Heuristic introduced
   for BVH split selection in
   [chapter 15](../03-scene-structure/15-spatial-acceleration.md).

5. <a id="5"></a>**Heckbert, P. S., Moreton, H. P.** (1991).
   "Interpolation for polygon texture mapping and shading."
   *State of the Art in Computer Graphics: Visualization and
   Modeling.* The 1/z perspective-correct interpolation trick
   used in [chapter 21](../04-rasterization/21-msaa-and-attribute-interpolation.md).

6. <a id="6"></a>**Sutherland, I. E., Hodgman, G. W.** (1974).
   "Reentrant Polygon Clipping." *Communications of the ACM,
   17(1).* The polygon-clipping algorithm used in
   [chapter 19](../04-rasterization/19-clipping-depth-stencil.md).

7. <a id="7"></a>**Gilbert, E. G., Johnson, D. W., Keerthi, S. S.**
   (1988). "A Fast Procedure for Computing the Distance Between
   Complex Objects in Three-Dimensional Space." *IEEE Journal
   of Robotics and Automation, 4(2).* The GJK distance algorithm
   used in [chapter 14](../03-scene-structure/14-csg.md).

## Tone mapping & color

8. <a id="8"></a>**Reinhard, E., Stark, M., Shirley, P., Ferwerda,
   J.** (2002). "Photographic Tone Reproduction for Digital
   Images." *Proceedings of SIGGRAPH '02.* The Reinhard tonemap
   operator. [Chapter 12](../02-ray-rendering/12-tone-mapping.md).

9. <a id="9"></a>**Narkowicz, K.** (2015). "ACES Filmic Tone
   Mapping Curve." *Blog post,
   https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/.*
   The polynomial fit shipped as the project's "ACES" operator.
   [Chapter 12](../02-ray-rendering/12-tone-mapping.md).

## See also

- [Top-level TOC](../README.md)
- [A. Glossary](a-glossary.md)
- [C. Source map](c-source-map.md)
