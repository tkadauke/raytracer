# Spatial acceleration

A scene with $N$ primitives, traced naively, costs $\mathcal{O}(N)$
per ray: every primitive gets tested against every ray. A scene
with 10 primitives takes 10 tests per ray; a scene with 10,000
primitives takes 10,000 tests per ray — a 1000× slowdown for the
same image. That scaling is unsustainable, and it is what
**spatial acceleration** structures exist to fix.

This chapter is about the two acceleration structures the
codebase ships:
[`BVH`](../../../include/render/primitives/BVH.h)
(Bounding Volume Hierarchy) and
[`Grid`](../../../include/render/primitives/Grid.h) (uniform 3D
grid). Both implement the explicit
[`SpatialIndex`](../../../include/render/primitives/SpatialIndex.h)
contract alongside the flat
[`Composite`](../../../include/render/primitives/Composite.h)
fallback. All three share the same add/setup/bounds/intersection
surface, but only BVH and Grid build acceleration data.
Both attack the same problem with different trade-offs,
and both reduce the per-ray cost to roughly
$\mathcal{O}(\log N)$ for typical scenes.

By the end of this chapter you should know:

- why $\mathcal{O}(N)$ per ray is unsustainable past a few
  hundred primitives,
- how a [BVH](../appendix/a-glossary.md#b)'s binary tree of bounding boxes prunes most of the
  scene per ray,
- the Surface Area Heuristic for picking BVH split axes and
  positions,
- how a uniform grid does the same job through 3D-[DDA](../appendix/a-glossary.md#d) traversal,
- which structure to pick for a given scene.

## <a id="the-cost-of-doing-nothing"></a>The cost of doing nothing
The unaccelerated scene primitive is
[`Composite`](../../../include/render/primitives/Composite.h):
a list of children with an `intersect` that just iterates them.
For a primary ray:

```cpp
const Primitive* hit = nullptr;
double bestT = infinity;
for (const auto& child : children) {
  HitPointInterval hp;
  auto p = child->intersect(ray, hp, state);
  if (p && hp.minT() < bestT) { hit = p; bestT = hp.minT(); }
}
```

One intersect call per child. For 100 children that's 100
calls; for 10,000 it's 10,000. Worse, every child does its own
work — a triangle's [Möller-Trumbore](../appendix/a-glossary.md#m) is cheap, but a torus's
quartic root finding ([Primitives and intersection: Torus: a quartic root problem](../ray-rendering/primitives-and-intersection.md#torus-a-quartic-root-problem))
is expensive, and running 10,000 of them per ray is enough to
melt a CPU.

The relevant observation is that *most* of those tests are
wasted. A ray pointed at a single sphere in the front-left
corner of the scene doesn't need to test against the 9,999
primitives in the rest of the scene; it only needs to test
against primitives whose region of space the ray actually
enters.

Acceleration structures formalize that observation. Both BVH
and Grid partition the primitives by spatial location, then
test only those primitives whose region the ray traverses. The
right structure for a given scene depends on whether the
spatial distribution is regular (Grid wins) or irregular (BVH
wins).

## <a id="the-bounding-volume-hierarchy"></a>The bounding volume hierarchy
A **BVH** is a binary tree where each internal node owns a
bounding box (an [AABB](../appendix/a-glossary.md#a) from
[Rays and geometry: Bounding boxes](../foundations/rays-and-geometry.md#bounding-boxes))
that tightly encloses all primitives in the subtree rooted at
that node. Each leaf node owns a small batch of primitives
(typically 4–16) that get tested directly against the ray.

The traversal algorithm is short. Given a ray:

1. Test the ray against the root node's AABB. If it misses,
   the entire scene misses; return.
2. If the ray hits, recurse into the children. For each child,
   test the ray against that child's AABB.
3. At a leaf, run a brute-force loop over the leaf's primitives.

The widget shows this step by step on a small example:

<!-- widget: bvh_sah_traversal -->

The asymptotics are $\mathcal{O}(\log N)$ per ray when the tree
is balanced and the scene's primitives are well-distributed.
Each level of the tree at most doubles the work, but each level
also at most halves the spatial extent the ray needs to cover —
so most rays only descend one branch at any given level. A
ray that hits a single leaf descends from root to leaf in
$\log_2 N$ steps; a ray that grazes a region with multiple
overlapping nodes might descend a few branches but never the
whole tree.

The two questions BVH construction has to answer are *where to
split* (which axis, at what position) and *when to stop*
(leaf size threshold).

## <a id="the-surface-area-heuristic"></a>The Surface Area Heuristic
The naive "split at the median" produces a perfectly balanced
tree but ignores the *shape* of the spatial distribution.
Consider a long, thin row of spheres along the x-axis: a
y-axis median split groups primitives that are close in y but
arbitrary in x, and the resulting AABBs at the top of the
tree overlap heavily. Rays end up testing both children of the
split, not just one — defeating the purpose.

The **Surface Area Heuristic** ([SAH](../appendix/a-glossary.md#s); Goldsmith & Salmon 1987,
refined by MacDonald & Booth 1990) picks the split that
minimizes the *expected* traversal cost. The cost model says:
for a node containing children with AABBs $\{A_i\}$ and total
node-AABB surface area $S_{\text{node}}$, splitting into a
left subtree and a right subtree costs

$$
C_{\text{split}} = C_t + \frac{S_L}{S_{\text{node}}} \, N_L \, C_i + \frac{S_R}{S_{\text{node}}} \, N_R \, C_i
$$

where $C_t$ is the traversal cost of one internal node, $C_i$
is the cost of testing one primitive, $N_L$ and $N_R$ are the
number of primitives on each side, and $S_L$ and $S_R$ are the
surface areas of the left and right child AABBs. The intuition
is that a randomly-oriented ray hits a child node with
probability proportional to that child's surface area
(Cauchy's formula); $S_L / S_{\text{node}}$ and $S_R /
S_{\text{node}}$ are the per-child hit probabilities.

The SAH-optimal split minimizes $C_{\text{split}}$ over the
candidate axis (longest centroid-bbox dimension) and split
positions ($N - 1$ candidates, one between each consecutive
pair of primitives sorted along the axis). The build evaluates
all candidates and picks the winner. If no split improves
$C_{\text{split}}$ over leaving the node as a leaf, the
recursion stops.

The codebase's
[`BVH::setup`](../../../include/render/primitives/BVH.h)
implements exactly this: longest-centroid-axis split, sorted-
then-swept SAH evaluation, recursion until either the leaf
budget is reached or no split improves the cost. This is the
standard "high-quality BVH" construction; binned SAH (a faster
approximation) is queued for future optimization but isn't
needed yet.

The performance contract is pinned by
[`test/unit/render/primitives/BVHPerformanceTest.cpp`](../../../test/unit/render/primitives/BVHPerformanceTest.cpp):
a flat-Composite scene of 200 spheres versus the same scene
wrapped in a BVH must show the BVH at least 5× faster on
primary rays. The 5× threshold is conservative — typical
speedups are 10–50× on scenes of this size — but the
ratio-assertion test tolerates the environmental noise
between debug-vs-release and CI-vs-developer-machine that an
absolute-time test wouldn't.

## <a id="the-uniform-grid-alternative"></a>The uniform grid alternative
A **Grid** partitions space into a regular 3D array of cells
of identical size, and bins each primitive into every cell its
bounding box touches. A ray's traversal walks the cells the
ray passes through, in order, testing the primitives in each
cell.

The widget shows the per-cell traversal step by step:

<!-- widget: grid_dda_traversal -->

The traversal algorithm is **3D-DDA** — Digital Differential
Analyzer. The 1D version is the [Bresenham](../appendix/a-glossary.md#b) line algorithm; the
3D version steps a ray through a uniform-grid cell array, at
each step picking the axis whose cell boundary the ray crosses
*next*. The math is one increment per axis per step, with no
divisions in the inner loop: a constant-time-per-cell
traversal that touches exactly the cells the ray's geometric
line passes through.

Construction is much faster than BVH: bin every primitive into
its overlapping cells, no SAH, no recursion. For a scene that
gets rebuilt every frame (animation, deformation), this is the
biggest practical advantage.

The downside is that the cells are uniform, so primitives of
wildly different sizes get binned poorly. A scene with one
giant ground plane and 10,000 small spheres will bin the
ground plane into every cell that touches it (effectively
every cell), making the per-cell test list ≥1 long
everywhere. A BVH builds a tight node around the ground plane
and tests against it once per ray.

The cell-count formula in
[`Grid::setup`](../../../include/render/primitives/Grid.h)
follows Cleary & Wyvill 1988: the cell count along each axis
is proportional to the cube root of the primitive count,
scaled by the spatial extent of the scene along that axis.
For a 1,000-primitive cube-shaped scene, that produces about
10 cells per axis — 1,000 cells total — for a target of
roughly one primitive per cell on average.

## <a id="bvh-versus-grid"></a>BVH versus Grid
The two structures address the same problem with different
trade-offs:

- **BVH wins on irregular distributions.** A scene with
  uneven primitive density (some clusters, some empty regions)
  builds tight AABBs around each cluster and prunes empty
  regions cheaply. A grid is forced to allocate cells in the
  empty regions too, where the per-cell test list is empty
  but the DDA traversal still has to step through them.
- **BVH wins on widely varying primitive sizes.** A grid bins
  large primitives into many cells; a BVH builds one tight
  node per large primitive.
- **BVH wins on static scenes.** The construction cost
  amortizes over many renders, and the SAH-optimal tree
  outperforms the grid by a measurable factor on typical
  scenes.
- **Grid wins on dynamic scenes.** Construction is faster, so
  rebuild cost per frame is lower.
- **Grid wins on dense uniform distributions.** Triangle soups
  of similar-sized triangles sometimes bin perfectly into the
  grid's cells, giving constant-time per-cell tests with no
  overhead.

The codebase's default is BVH. `Grid` ships as the
educational alternative.

## <a id="measured-policy-defaults"></a>Measured policy defaults
Automatic acceleration is intentionally conservative at the
edges and opinionated for real scenes:

- empty and single-leaf scenes use the Linear fallback, avoiding
  accelerator setup when there is nothing useful to accelerate;
- multi-leaf scenes default to BVH;
- Grid remains an explicit mode for regular, primary-heavy
  scenes where uniform cells are known to match the geometry.

The default is backed by the policy benchmark in
[`benchmarks/AccelerationPolicyBenchmark.cpp`](../../../benchmarks/AccelerationPolicyBenchmark.cpp),
with the recorded run in
[`docs/perf/acceleration-policy-benchmark-2026-05-28.md`](../../perf/acceleration-policy-benchmark-2026-05-28.md).
That benchmark compares Linear, Grid, and BVH on procedural
clusters, a mesh-heavy terrain, an imported PLY triangle soup,
and a repeated imported-assembly-style box scene. It records
index build time, closest-hit primary rays, boolean shadow rays,
and a primary-ray render-impact proxy.

The measurements show three policy facts:

- Linear builds fastest, but on multi-leaf ray queries it is
  orders of magnitude slower than either accelerator. This backs
  Linear only as the empty/single-leaf fallback.
- Grid is excellent for regular primary-ray workloads. It wins
  the primary render proxy on the procedural cluster, terrain,
  and repeated-box assembly workloads, so it remains a first-class
  manual policy for scenes with regular spatial density.
- BVH wins every measured shadow-ray workload and wins the
  imported PLY render proxy. Whitted-style renders trace shadow
  rays from visible surfaces, and static imported scenes amortize
  setup cost across many queries, so Auto uses BVH as the
  multi-leaf default.

## <a id="acceleration-is-invisible-at-the-pixel-level"></a>Acceleration is invisible at the pixel level
The crucial property of any acceleration structure is that it
*doesn't change what gets rendered*. The same scene wrapped in
a BVH versus wrapped in a flat Composite produces the same
pixels — same hit points, same shading, same image. The
structure is a per-ray *speedup*, not a *correctness* mechanism.

This is why the BVH and Grid widgets show traversal patterns
rather than rendered output: there is no rendered output that
visibly distinguishes BVH from Grid from no-acceleration. The
only difference is how long the render takes.

This property also makes acceleration testing easier than it
might seem. A correctness test is "does the BVH-wrapped scene
produce the same pixels as the flat-Composite scene?", which
is a byte-comparison of two render buffers. A performance
test is "does the BVH-wrapped scene render faster than the
flat-Composite scene by at least 5×?", which is the ratio
assertion the
[`BVHPerformanceTest`](../../../test/unit/render/primitives/BVHPerformanceTest.cpp)
performs. Both are simple to express; both run as part of the
unit test suite without needing instrumentation or
benchmark-grade timing infrastructure.

## <a id="what-this-chapter-does-not-cover"></a>What this chapter does *not* cover
Several acceleration structures are not implemented:

- **kd-tree.** Like BVH, a binary tree, but the splitting plane
  is axis-aligned and *contiguous* — the left and right child
  partition the parent's AABB, no overlap. Better than BVH on
  some scenes (mesh-heavy game levels) but worse on others
  (instanced primitives where the children's AABBs don't
  align with axis planes).
- **Octree.** 3D analogue of a quadtree — each internal node
  has eight children, partitioning its AABB into eight equal
  octants. Common in voxel-grid scenes; less
  common for ray tracing today.
- **BVH refinement.** Tree quality improvements over the
  initial SAH build (treelet rotation, repeated SAH sweeps,
  spatial splits). Used by Embree and other production
  ray-tracing libraries.

All four are educational territory the codebase will likely
cover. None ship today; the BVH-vs-Grid pair is enough to
illustrate the spatial-acceleration *idea*, and adding more
structures is purely a question of implementation effort.

## <a id="exercises"></a>Exercises
1. Predict the BVH traversal cost for a ray pointed at a
   single sphere in a scene of 1,000 spheres uniformly
   distributed in a unit cube. Predict the cost for a ray
   that misses every sphere. Run both predictions through the
   $\mathcal{O}(\log N)$ asymptote and explain which case is
   closer.
2. Construct a scene where Grid outperforms BVH. Hint: think
   about the regularity of the primitive distribution.
3. Read the SAH cost formula. What happens to
   $C_{\text{split}}$ when the split puts all primitives on
   one side of the axis? Why does the SAH naturally avoid
   that "split"?
4. The performance test asserts BVH is at least 5× faster than
   flat-Composite. What scene properties would make the actual
   ratio 50× (well above the threshold), versus 2× (below the
   threshold)? Where would each scene type live in a real
   workload?

## See also

- Volume index: [Scene structure](README.md)
- Previous:
  [Constructive solid geometry](csg.md)
- Next:
  [Instances and motion blur](instances-and-motion-blur.md)
- Bounding-box vocabulary:
  [Bounding boxes](../foundations/rays-and-geometry.md#bounding-boxes)
- Per-primitive intersection:
  [Primitives and intersection](../ray-rendering/primitives-and-intersection.md)
- Performance test (5× ratio assertion):
  [`test/unit/render/primitives/BVHPerformanceTest.cpp`](../../../test/unit/render/primitives/BVHPerformanceTest.cpp)

## Source anchors

<!-- source-anchors -->
- `include/render/primitives/SpatialIndex.h`
- `include/render/primitives/Composite.h`
- `include/render/primitives/BVH.h`
- `include/render/primitives/Grid.h`
- `include/core/math/BoundingBox.h`
- `benchmarks/AccelerationPolicyBenchmark.cpp`
- `docs/perf/acceleration-policy-benchmark-2026-05-28.md`
- `test/unit/render/primitives/BVHPerformanceTest.cpp`
<!-- /source-anchors -->
