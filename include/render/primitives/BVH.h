#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/math/BoundingBox.h"
#include "render/primitives/Composite.h"

namespace render {
  /**
    * @brief Bounding-Volume-Hierarchy spatial accelerator — `Composite`
    *        that partitions its child primitives into a binary tree
    *        of axis-aligned bounding boxes.
    *
    * Classic BVH structure: each internal node owns two children and
    * an AABB tight around all primitives below it; each leaf node
    * owns a small batch of primitives (default 4) to amortize the
    * traversal overhead. `intersect` walks the tree, descending only
    * into nodes whose AABB the ray hits.
    *
    * Build is one-shot via `setup()`: takes the children added so far,
    * computes their AABBs, recursively splits via the **Surface Area
    * Heuristic** (SAH — Goldsmith & Salmon 1987, refined by MacDonald
    * & Booth 1990) until either the leaf budget is reached or no
    * split improves the expected traversal cost. The split axis is
    * the longest dimension of the centroid bounding box at each
    * level, with N-1 candidate split positions evaluated per recursion
    * (sorted-then-swept; binned SAH is a future optimization).
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="bvh_sah_traversal.js"></script>
    * @endhtmlonly
    *
    * Use `BVH` instead of `Grid` when:
    *
    *  - The scene has more than ~100 primitives.
    *  - Primitive sizes vary widely (Grid's uniform cells handle
    *    this poorly; BVH's adaptive AABBs don't).
    *  - The scene is largely static and `setup()` cost is amortised
    *    over many renders.
    *
    * Use `Grid` when build speed matters more than traversal speed
    * (animation rebuilds, very dense triangle soups where the
    * BVH-build cost dominates).
    *
    * @see Composite — unaccelerated parent.
    * @see Grid — uniform-cell alternative for the same use case.
    */
  class BVH : public Composite {
  public:
    BVH() = default;
    ~BVH() override;

    /**
      * Recursive descent of the BVH tree. Skips entire subtrees whose
      * AABB the ray misses. Returns the closest-hit primitive across
      * the whole tree, with `hitPoints` populated by every primitive
      * that intersected (matches `Composite`'s contract).
      */
    const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                               render::State& state) const override;

    /**
      * Boolean shadow-ray variant — same descent but short-circuits
      * on the first leaf-primitive hit.
      */
    bool intersects(const Rayd& ray, render::State& state) const override;

    /**
      * Block-batched packet traversal for four rays. Walks all lanes
      * through a single BVH descent, pruning subtrees where every
      * active-mask ray misses the node AABB. Leaf hits are accumulated
      * per-lane via Primitive::intersectPacket (SSE path for Sphere,
      * scalar fallback for other primitives). Returns tNear set to the
      * closest positive hit distance per lane; tFar mirrors tNear.
      *
      * Coherent rays (same pixel tile, nearly identical directions)
      * follow the same BVH path and benefit from both tree-level cache
      * reuse and leaf-level SIMD parallelism. Incoherent rays (random
      * directions) produce sparse active masks and fall back toward
      * scalar cost.
      */
    RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const override;

#ifdef __AVX__
    /**
      * Eight-lane variant. Active-mask descent is identical to the
      * Ray4 path; leaf dispatch calls Primitive::intersectPacket(Ray8).
      * Only compiled when the toolchain has AVX enabled.
      */
    RayPacketIntersection8 intersectPacket(const Ray8& rays, render::State& state) const override;
#endif

    /**
      * Build the BVH from the children added so far. Must be called
      * after all `add()` calls and before the first `intersect`.
      * Re-calling rebuilds from scratch.
      */
    void setup() override;

    /**
      * Tunable: max primitives per leaf node. Smaller values produce
      * deeper trees with more bbox-test overhead but tighter culling;
      * larger values produce shallower trees with more
      * primitive-intersection work per leaf hit. Default 4 matches
      * the PBRT recommendation.
      */
    inline void setLeafSize(int n) {
      m_leafSize = n;
    }
    inline int leafSize() const {
      return m_leafSize;
    }

  private:
    struct Node {
      BoundingBoxd bbox;
      std::unique_ptr<Node> left;
      std::unique_ptr<Node> right;
      std::vector<std::shared_ptr<Primitive>> primitives;

      inline bool isLeaf() const {
        return !left && !right;
      }
    };

    std::unique_ptr<Node> m_root;
    int m_leafSize{4};

    std::unique_ptr<Node> build(std::vector<std::shared_ptr<Primitive>> prims) const;
    const Primitive* intersectNode(const Node* node, const Rayd& ray,
                                   const Vector3d& inverseDirection, HitPointInterval& hitPoints,
                                   render::State& state) const;
    const Primitive* intersectHitNode(const Node* node, const Rayd& ray,
                                      const Vector3d& inverseDirection, HitPointInterval& hitPoints,
                                      render::State& state) const;
    bool intersectsNode(const Node* node, const Rayd& ray, const Vector3d& inverseDirection,
                        render::State& state) const;
    bool intersectsHitNode(const Node* node, const Rayd& ray, const Vector3d& inverseDirection,
                           render::State& state) const;

    void intersectPacketNode(const Node* node, const Ray4& rays, uint16_t activeMask,
                             std::array<float, Ray4::lanes>& tMin, uint16_t& hitMask,
                             render::State& state) const;
#ifdef __AVX__
    void intersectPacketNode(const Node* node, const Ray8& rays, uint16_t activeMask,
                             std::array<float, Ray8::lanes>& tMin, uint16_t& hitMask,
                             render::State& state) const;
#endif
  };
}
