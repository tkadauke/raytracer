#pragma once

#include "core/math/Vector.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <utility>

namespace render {
  class HomogeneousClipPlane {
  public:
    // The rasterizer clips against eye-relative depth and the normalized
    // viewport in one homogeneous pass before x/w and y/w are converted to
    // pixels. A far depth of infinity makes the Far plane accept everything.
    enum Kind { Near, Left, Right, Top, Bottom, Far };

    constexpr HomogeneousClipPlane(Kind kind, double nearDepth, double farDepth)
        : m_kind(kind),
          m_nearDepth(nearDepth),
          m_farDepth(farDepth) {
    }

    constexpr std::uint8_t bit() const {
      return static_cast<std::uint8_t>(1u << static_cast<int>(m_kind));
    }

    double distance(const Vector4d& clip) const {
      switch (m_kind) {
      case Near:
        return clip.z() - m_nearDepth;
      case Left:
        return clip.x() + clip.w();
      case Right:
        return clip.w() - clip.x();
      case Top:
        return clip.y() + clip.w();
      case Bottom:
        return clip.w() - clip.y();
      case Far:
        return m_farDepth - clip.z();
      }
      return -1.0;
    }

    bool contains(const Vector4d& clip) const {
      return distance(clip) >= 0.0;
    }

  private:
    Kind m_kind;
    double m_nearDepth;
    double m_farDepth;
  };

  class HomogeneousClipVolume {
  public:
    explicit HomogeneousClipVolume(double nearDepth,
                                   double farDepth = std::numeric_limits<double>::infinity())
        : m_planes({{HomogeneousClipPlane(HomogeneousClipPlane::Near, nearDepth, farDepth),
                     HomogeneousClipPlane(HomogeneousClipPlane::Left, nearDepth, farDepth),
                     HomogeneousClipPlane(HomogeneousClipPlane::Right, nearDepth, farDepth),
                     HomogeneousClipPlane(HomogeneousClipPlane::Top, nearDepth, farDepth),
                     HomogeneousClipPlane(HomogeneousClipPlane::Bottom, nearDepth, farDepth),
                     HomogeneousClipPlane(HomogeneousClipPlane::Far, nearDepth, farDepth)}}) {
    }

    static constexpr std::uint8_t allBits() {
      return static_cast<std::uint8_t>((1u << 6) - 1u);
    }

    std::uint8_t outCode(const Vector4d& clip) const {
      if (clip.isUndefined()) {
        return allBits();
      }

      std::uint8_t outCode = 0;
      for (const HomogeneousClipPlane& plane : m_planes) {
        if (!plane.contains(clip)) {
          outCode |= plane.bit();
        }
      }
      return outCode;
    }

    template<class Vertex, std::size_t MaxVertices, class ClipFn, class InterpolateFn>
    std::size_t clipTriangle(const std::array<Vertex, 3>& input,
                             std::array<Vertex, MaxVertices>& clipped, ClipFn clipOf,
                             InterpolateFn interpolate) const {
      static_assert(MaxVertices >= 9, "clipped output must fit a triangle clipped by six planes");

      bool allInside = true;
      for (const HomogeneousClipPlane& plane : m_planes) {
        std::size_t insideCount = 0;
        for (const Vertex& vertex : input) {
          if (plane.contains(clipOf(vertex))) {
            ++insideCount;
          }
        }
        if (insideCount == 0)
          return 0;
        if (insideCount != input.size()) {
          allInside = false;
        }
      }

      if (allInside) {
        for (std::size_t i = 0; i < input.size(); ++i) {
          clipped[i] = input[i];
        }
        return input.size();
      }

      std::array<Vertex, MaxVertices> polygonA;
      std::array<Vertex, MaxVertices> polygonB;
      for (std::size_t i = 0; i < input.size(); ++i) {
        polygonA[i] = input[i];
      }

      std::size_t count = input.size();
      auto* in = &polygonA;
      auto* out = &polygonB;

      for (const HomogeneousClipPlane& plane : m_planes) {
        count = clipPolygonAgainstPlane(*in, count, plane, *out, clipOf, interpolate);
        if (count < 3)
          return 0;
        std::swap(in, out);
      }

      for (std::size_t i = 0; i < count; ++i) {
        clipped[i] = (*in)[i];
      }
      return count;
    }

  private:
    template<class Vertex, std::size_t MaxVertices, class ClipFn, class InterpolateFn>
    std::size_t clipPolygonAgainstPlane(const std::array<Vertex, MaxVertices>& input,
                                        std::size_t inputCount, const HomogeneousClipPlane& plane,
                                        std::array<Vertex, MaxVertices>& output, ClipFn clipOf,
                                        InterpolateFn interpolate) const {
      if (inputCount == 0)
        return 0;

      std::size_t outputCount = 0;
      Vertex prev = input[inputCount - 1];
      double prevDistance = plane.distance(clipOf(prev));
      bool prevInside = prevDistance >= 0.0;

      for (std::size_t i = 0; i < inputCount; ++i) {
        const Vertex& curr = input[i];
        const double currDistance = plane.distance(clipOf(curr));
        const bool currInside = currDistance >= 0.0;

        if (currInside != prevInside) {
          const double t = prevDistance / (prevDistance - currDistance);
          output[outputCount++] = interpolate(prev, curr, t);
        }
        if (currInside) {
          output[outputCount++] = curr;
        }

        prev = curr;
        prevDistance = currDistance;
        prevInside = currInside;
      }

      return outputCount;
    }

    std::array<HomogeneousClipPlane, 6> m_planes;
  };
}
