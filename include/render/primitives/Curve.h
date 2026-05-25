#pragma once

#include "core/geometry/AttributeColorMap.h"
#include "core/geometry/Polyline.h"
#include "render/primitives/Primitive.h"

#include <optional>

namespace render {
  /**
    * Runtime polyline curve primitive for mesh-based engines.
    *
    * `Curve` makes path geometry visible to engines that consume
    * `Primitive::tessellate()`. The ray-intersection path currently reports
    * a miss, while wireframe/raster/export-style consumers can request either
    * flat ribbons or round tube meshes for finite-width polylines.
    */
  class Curve : public Primitive {
  public:
    enum class TessellationMode {
      Ribbon,
      Tube
    };

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

    [[nodiscard]] inline const std::optional<core::AttributeColorMap>& segmentColorMap() const
      noexcept {
      return m_segmentColorMap;
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;

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
