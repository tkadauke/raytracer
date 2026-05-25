#pragma once

#include "core/geometry/Curve.h"
#include "core/math/BoundingBox.h"

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace core {

  /**
    * Ordered 3D point path with optional per-segment metadata.
    *
    * Segment i connects point i to point i + 1. The segment metadata vector is
    * maintained in lockstep with that derived segment count, so empty and
    * single-point polylines always expose zero segments.
    *
    * `Polyline` is the core data shape for imported paths: G-code toolpaths,
    * molecular bonds or backbones, simulation trajectories, GPS routes, and
    * other sampled curves can all be represented as ordered 3D points plus
    * typed attributes. Whole-path metadata is inherited from `core::Curve`;
    * use per-segment attributes for values that should affect rendering one
    * span at a time, such as:
    *
    *  - scalar values (`double` or `int`) for feed rate, temperature, speed,
    *    height, time, residue index, or confidence;
    *  - categorical values (`std::string`, `bool`, or `int`) for travel/cut
    *    moves, molecule chain labels, transport mode, route class, or phase;
    *  - vector values (`Vector3d`) for importer-specific coordinates or
    *    directions that later processing wants to preserve.
    *
    * Rendering is handled by `render::Curve`: zero-width polylines can be
    * drawn as overlay center lines, and finite-width polylines can be
    * tessellated into ribbons or tubes for mesh-consuming engines.
    */
  class Polyline : public Curve {
  public:
    struct Segment {
      std::size_t index;
      const Vector3d& start;
      const Vector3d& end;
      const AttributeMap& attributes;
    };

    class SegmentIterator {
    public:
      using iterator_category = std::forward_iterator_tag;
      using value_type = Segment;
      using difference_type = std::ptrdiff_t;
      using pointer = void;
      using reference = Segment;

      [[nodiscard]] inline Segment operator*() const {
        return Segment{m_index, m_polyline->point(m_index), m_polyline->point(m_index + 1),
                       m_polyline->segmentAttributes(m_index)};
      }

      inline SegmentIterator& operator++() {
        ++m_index;
        return *this;
      }

      [[nodiscard]] inline SegmentIterator operator++(int) {
        SegmentIterator copy(*this);
        ++(*this);
        return copy;
      }

      [[nodiscard]] inline bool operator==(const SegmentIterator& other) const noexcept {
        return m_polyline == other.m_polyline && m_index == other.m_index;
      }

      [[nodiscard]] inline bool operator!=(const SegmentIterator& other) const noexcept {
        return !(*this == other);
      }

    private:
      friend class Polyline;

      inline SegmentIterator(const Polyline& polyline, std::size_t index)
          : m_polyline(&polyline),
            m_index(index) {
      }

      const Polyline* m_polyline;
      std::size_t m_index;
    };

    inline Polyline() = default;

    inline explicit Polyline(const std::vector<Vector3d>& points)
        : m_points(points),
          m_segmentAttributes(segmentCountFor(points.size())) {
    }

    inline Polyline(std::initializer_list<Vector3d> points)
        : m_points(points),
          m_segmentAttributes(segmentCountFor(m_points.size())) {
    }

    [[nodiscard]] inline bool empty() const noexcept {
      return m_points.empty();
    }

    [[nodiscard]] inline std::size_t pointCount() const noexcept {
      return m_points.size();
    }

    [[nodiscard]] inline std::size_t segmentCount() const noexcept {
      return segmentCountFor(m_points.size());
    }

    [[nodiscard]] inline const std::vector<Vector3d>& points() const noexcept {
      return m_points;
    }

    [[nodiscard]] inline const Vector3d& point(std::size_t index) const {
      return m_points.at(index);
    }

    inline void setPoints(const std::vector<Vector3d>& points) {
      m_points = points;
      m_segmentAttributes.resize(segmentCount());
    }

    inline void addPoint(const Vector3d& point) {
      m_points.push_back(point);
      m_segmentAttributes.resize(segmentCount());
    }

    [[nodiscard]] inline const AttributeMap& segmentAttributes(std::size_t segmentIndex) const {
      return m_segmentAttributes.at(segmentIndex);
    }

    [[nodiscard]] inline bool hasSegmentAttribute(std::size_t segmentIndex,
                                                  const std::string& name) const {
      const auto& attributes = segmentAttributes(segmentIndex);
      return attributes.find(name) != attributes.end();
    }

    [[nodiscard]] inline const AttributeValue* segmentAttribute(std::size_t segmentIndex,
                                                                const std::string& name) const {
      const auto& attributes = segmentAttributes(segmentIndex);
      const auto it = attributes.find(name);
      if (it == attributes.end())
        return nullptr;
      return &it->second;
    }

    template<class T>
    [[nodiscard]] inline const T* segmentAttributeAs(std::size_t segmentIndex,
                                                     const std::string& name) const {
      const auto* value = segmentAttribute(segmentIndex, name);
      if (value == nullptr)
        return nullptr;
      return std::get_if<T>(value);
    }

    inline void setSegmentAttribute(std::size_t segmentIndex, const std::string& name,
                                    const AttributeValue& value) {
      m_segmentAttributes.at(segmentIndex)[name] = value;
    }

    inline bool removeSegmentAttribute(std::size_t segmentIndex, const std::string& name) {
      return m_segmentAttributes.at(segmentIndex).erase(name) != 0;
    }

    inline void clearSegmentAttributes(std::size_t segmentIndex) {
      m_segmentAttributes.at(segmentIndex).clear();
    }

    [[nodiscard]] inline BoundingBoxd bounds() const {
      BoundingBoxd box;
      for (const auto& point : m_points)
        box.include(point);
      return box;
    }

    [[nodiscard]] inline SegmentIterator begin() const {
      return SegmentIterator(*this, 0);
    }

    [[nodiscard]] inline SegmentIterator end() const {
      return SegmentIterator(*this, segmentCount());
    }

  private:
    [[nodiscard]] inline static std::size_t segmentCountFor(std::size_t pointCount) noexcept {
      if (pointCount < 2)
        return 0;
      return pointCount - 1;
    }

    std::vector<Vector3d> m_points;
    std::vector<AttributeMap> m_segmentAttributes;
  };

} // namespace core
