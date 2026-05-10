# Appendix B — Bibliography

Foundational citations the book draws on. Numbered for stable
in-text references — chapter prose links to specific entries
as `[Pineda 1988](b-bibliography.md#1)`.

Hand-written; *not* generated. New citations get appended; the
numeric keys never get reused so existing in-text references
keep working.

## Foundational rendering

1. <a id="1"></a>**Pineda, J.** (1988). "A Parallel Algorithm
   for Polygon Rasterization." *Proceedings of SIGGRAPH '88,
   Computer Graphics 22(4),* 17–20. The edge-function
   rasterization algorithm used in
   [chapter 18](../04-rasterization/18-the-rasterization-pipeline.md).

2. <a id="2"></a>**Whitted, T.** (1980). "An Improved
   Illumination Model for Shaded Display." *Communications
   of the ACM, 23(6),* 343–349. The recursive ray-tracing
   algorithm that names
   [chapter 5](../02-ray-rendering/05-the-whitted-pipeline.md).

3. <a id="3"></a>**Möller, T., Trumbore, B.** (1997).
   "Fast, Minimum Storage Ray-Triangle Intersection."
   *Journal of Graphics Tools, 2(1),* 21–28. The triangle-
   intersection algorithm used in
   [chapter 7 §7.5](../02-ray-rendering/07-primitives-and-intersection.md#7-5-triangle-moller-trumbore).

4. <a id="4"></a>**MacDonald, J. D., Booth, K. S.** (1990).
   "Heuristics for ray tracing using space subdivision."
   *The Visual Computer, 6(3),* 153–166. The Surface Area
   Heuristic refined for BVH split selection in
   [chapter 15 §15.3](../03-scene-structure/15-spatial-acceleration.md#15-3-the-surface-area-heuristic).

5. <a id="5"></a>**Heckbert, P. S., Moreton, H. P.** (1991).
   "Interpolation for Polygon Texture Mapping and Shading."
   In *State of the Art in Computer Graphics: Visualization
   and Modeling.* Springer-Verlag. The $1/z$ perspective-
   correct interpolation trick used in
   [chapter 21 §21.2](../04-rasterization/21-msaa-and-attribute-interpolation.md#21-2-the-heckbert-moreton-1z-trick).

6. <a id="6"></a>**Sutherland, I. E., Hodgman, G. W.**
   (1974). "Reentrant Polygon Clipping." *Communications of
   the ACM, 17(1),* 32–42. The polygon-clipping algorithm
   used in
   [chapter 19 §19.2](../04-rasterization/19-clipping-depth-stencil.md#19-2-sutherland-hodgman-in-homogeneous-clip-space).

7. <a id="7"></a>**Gilbert, E. G., Johnson, D. W.,
   Keerthi, S. S.** (1988). "A Fast Procedure for Computing
   the Distance Between Complex Objects in Three-
   Dimensional Space." *IEEE Journal of Robotics and
   Automation, 4(2),* 193–203. The GJK distance algorithm
   used in
   [chapter 14 §14.4](../03-scene-structure/14-csg.md#14-4-gjk-ray-intersection-on-a-support-function).

## Tone mapping and color

8. <a id="8"></a>**Reinhard, E., Stark, M., Shirley, P.,
   Ferwerda, J.** (2002). "Photographic Tone Reproduction
   for Digital Images." *ACM Transactions on Graphics,
   21(3),* 267–276. The Reinhard tonemap operator $y = x /
   (1 + x)$.
   [Chapter 12 §12.4](../02-ray-rendering/12-tone-mapping.md#12-4-reinhard-compressed-everywhere).

9. <a id="9"></a>**Narkowicz, K.** (2015). "ACES Filmic
   Tone Mapping Curve." Blog post,
   *https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/*.
   The polynomial fit shipped as the project's "ACES"
   operator.
   [Chapter 12 §12.5](../02-ray-rendering/12-tone-mapping.md#12-5-aces-filmic-ish-punchy-midtones).

## Spatial acceleration

10. <a id="10"></a>**Goldsmith, J., Salmon, J.** (1987).
    "Automatic Creation of Object Hierarchies for Ray
    Tracing." *IEEE Computer Graphics and Applications,
    7(5),* 14–20. The original BVH construction algorithm
    underpinning the SAH refinement in [4].
    [Chapter 15 §15.2](../03-scene-structure/15-spatial-acceleration.md#15-2-the-bounding-volume-hierarchy).

11. <a id="11"></a>**Cleary, J. G., Wyvill, G.** (1988).
    "Analysis of an algorithm for fast ray tracing using
    uniform space subdivision." *The Visual Computer, 4(2),*
    65–83. The cube-root-of-N grid-cell-count formula used
    by `Grid::setup` in
    [chapter 15 §15.4](../03-scene-structure/15-spatial-acceleration.md#15-4-the-uniform-grid-alternative).

## Rasterization

12. <a id="12"></a>**Bresenham, J. E.** (1965). "Algorithm
    for computer control of a digital plotter." *IBM Systems
    Journal, 4(1),* 25–30. The integer-only line
    rasterization algorithm used by the wireframe engine in
    [chapter 20 §20.2](../04-rasterization/20-wireframe-rendering.md#20-2-bresenhams-line-algorithm).

## Sampling

13. <a id="13"></a>**Cook, R. L.** (1986). "Stochastic
    Sampling in Computer Graphics." *ACM Transactions on
    Graphics, 5(1),* 51–72. The foundational case for
    Monte Carlo super-sampling over regular sampling, with
    the variance and aliasing analyses underlying
    [chapter 10](../02-ray-rendering/10-sampling-and-anti-aliasing.md).

14. <a id="14"></a>**Mitchell, D. P.** (1991). "Spectrally
    Optimal Sampling for Distribution Ray Tracing."
    *Computer Graphics, 25(4),* 157–164. The pattern-
    optimization analysis for jittered sampling that
    motivates the stratification-vs-random comparison in
    [chapter 10 §10.3](../02-ray-rendering/10-sampling-and-anti-aliasing.md#10-3-the-stratification-invariant).

## Computer vision

15. <a id="15"></a>**Polsby, D. D., Popper, R. D.** (1991).
    "The Third Criterion: Compactness as a Procedural
    Safeguard Against Partisan Gerrymandering." *Yale Law &
    Policy Review, 9(2),* 301–353. The geographic-shape
    compactness measure $4\pi \cdot \text{area} /
    \text{perimeter}^2$, repurposed as a CV shape
    descriptor in
    [chapter 23 §23.3](../05-image-and-vision/23-blob-analysis-and-silhouettes.md#23-3-what-a-blob-carries).
    The choice to credit a *legal* paper for a graphics
    descriptor reflects the descriptor's actual provenance —
    same formula, different field.

## Mesh and file formats

16. <a id="16"></a>**Turk, G.** (1994). "The PLY Polygon
    File Format." Stanford Computer Graphics Laboratory
    technical document. The PLY format reference used by
    [chapter 25](../06-tools-and-io/25-ply-parsing.md). Not
    formally published as a paper; the canonical reference
    is the Stanford lab's documentation.

## See also

- [Top-level TOC](../README.md)
- [A. Glossary](a-glossary.md)
- [C. Source map](c-source-map.md)
