#pragma once

#include "core/geometry/AttributeColorMap.h"
#include "core/geometry/Polyline.h"
#include "render/primitives/Primitive.h"

#include <optional>

namespace render {
  /**
    * Runtime polyline curve primitive for mesh-based and overlay engines.
    *
    * `Curve` makes path geometry visible to render paths that cannot
    * intersect mathematical curves directly. The ray-intersection path
    * currently reports a miss, while raster, wireframe, export-style, and
    * graph-overlay consumers use one of these modes:
    *
    *  - **Polyline overlay**: `forEachCurveOverlaySegment()` exposes the
    *    original center-line segments without requiring a physical width.
    *    This is the right mode for debug overlays, route traces, toolpath
    *    previews, trajectories, and other line data that should stay thin in
    *    image space.
    *  - **Ribbon tessellation**: finite-width curves emit one camera-facing
    *    quad per non-zero segment. Ribbons are cheap, readable for flat
    *    routes and 2D toolpaths, and useful when downstream engines consume
    *    ordinary mesh faces.
    *  - **Tube tessellation**: finite-width curves emit round tube segments.
    *    Tubes carry actual volume in the mesh approximation and are better for
    *    molecule bonds, 3D trajectories, and paths viewed from many angles.
    *  - **Attribute-color rendering**: when `setSegmentColorMap()` is set,
    *    scalar or categorical segment attributes from `core::Polyline` are
    *    mapped to per-segment colors. Missing attributes fall back to the
    *    curve's material/default color instead of inventing a color.
    *
    * The underlying data remains a `core::Polyline`, so importers can reuse
    * the same point/attribute fixtures for G-code, molecules, trajectories,
    * and route-like datasets, then choose the visual mode at render time.
    */
  class Curve : public Primitive {
  public:
    enum class TessellationMode { Ribbon, Tube };

    inline explicit Curve(const core::Polyline& polyline, double width = 0.0,
                          TessellationMode mode = TessellationMode::Ribbon)
        : m_polyline(polyline),
          m_width(width),
          m_mode(mode) {
    }

    [[nodiscard]] inline const core::Polyline& polyline() const noexcept {
      return m_polyline;
    }

    [[nodiscard]] inline double width() const noexcept {
      return m_width;
    }

    [[nodiscard]] inline TessellationMode tessellationMode() const noexcept {
      return m_mode;
    }

    inline void setSegmentColorMap(const core::AttributeColorMap& colorMap) {
      m_segmentColorMap = colorMap;
    }

    inline void clearSegmentColorMap() {
      m_segmentColorMap.reset();
    }

    [[nodiscard]] inline const std::optional<core::AttributeColorMap>&
    segmentColorMap() const noexcept {
      return m_segmentColorMap;
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;

    void forEachCurveOverlaySegment(const CurveOverlaySegmentVisitor& visitor) const override;

    /**
      * Tessellates non-zero-length segments into visible faces.
      *
      * Ribbon mode emits one quad per segment. Tube mode emits a ring pair per
      * segment, connected by quads; `lod` doubles the ring resolution from an
      * eight-sided tube at lod=0. Segments whose endpoints coincide are skipped.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    core::Polyline m_polyline;
    double m_width;
    TessellationMode m_mode;
    std::optional<core::AttributeColorMap> m_segmentColorMap;
  };
}
