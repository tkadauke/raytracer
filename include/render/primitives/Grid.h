#pragma once

#include <vector>
#include <memory>
#include <vector>

#include "render/primitives/Composite.h"

namespace render {
  /**
    * @brief Spatial-acceleration `Composite` — partitions space
    *        into a regular grid of cells and DDA-traverses cells
    *        the ray passes through.
    *
    * `Grid` is `Composite` plus an N×M×K cell array. After all
    * children are added, call `setup()` to size the grid (cell
    * count picked from the cube root of the primitive count, per
    * Cleary & Wyvill 1988) and bin each child into the cells its
    * bounding box overlaps. `intersect` then runs a 3D-DDA
    * traversal from the ray's bounding-box entry point, testing
    * only the children in each cell as it steps.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="grid_dda_traversal.js"></script>
    * @endhtmlonly
    *
    * Use Grid instead of plain `Composite` when:
    *
    *  - The composite has more than ~50 children (the linear scan
    *    cost dominates).
    *  - The children are spread across space (every cell has a few
    *    children, not all clustered in one).
    *  - You can call `setup()` once after construction (Grid
    *    doesn't auto-rebuild on `add()`).
    *
    * The world-to-runtime conversion path uses Grid for `Mesh`
    * primitives (each triangle gets its own cell binning) but
    * leaves regular composites as plain `Composite` — meshes
    * routinely have thousands of triangles where the speedup is
    * dramatic; small CSG groups don't benefit.
    *
    * @see Composite — the unaccelerated parent class.
    */
  class Grid : public Composite {
  public:
    inline Grid()
        : m_numX(0),
          m_numY(0),
          m_numZ(0) {
    }

    /**
      * 3D-DDA traversal: enters the grid at the ray's bounding-box
      * entry point, steps through cells in the ray direction,
      * intersects only the primitives in each cell, and returns
      * the closest hit. Returns the hit child, with `hitPoints`
      * populated by the children that actually intersected.
      */
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;

    /**
      * Boolean shadow-ray variant — same DDA traversal but
      * short-circuits on the first hit.
      */
    virtual bool intersects(const Rayd& ray, render::State& state) const override;

    /**
      * Build the cell grid from the children added so far. Cell
      * count is picked via `cbrt(numChildren)` along each axis,
      * scaled by the bounding-box aspect; each child is binned
      * into every cell its bounding box overlaps (so primitives
      * straddling a cell boundary appear in both).
      *
      * Must be called after all `add()` calls and before the
      * first `intersect`. Re-calling it rebuilds from scratch —
      * use this if you need to mutate the children at runtime,
      * though the typical pattern is to construct the grid fresh
      * for each render.
      */
    void setup() override;

  private:
    PrimitivePacketHit4
    intersectRay4PacketHitsThroughDda(const Ray4& rays, const PrimitivePacketState4& states) const;
    PrimitivePacketHit8
    intersectRay8PacketHitsThroughDda(const Ray8& rays, const PrimitivePacketState8& states) const;

    std::vector<std::shared_ptr<Primitive>> m_cells;
    int m_numX, m_numY, m_numZ;

    BoundingBoxd m_boundingBox;
  };
}
