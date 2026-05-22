#pragma once

#include "RasterPipelineTypes.h"
#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "core/math/BoundingBox.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"
#include "render/cameras/Camera.h"
#include "render/lights/Light.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace engine::raster::detail {

  // Orthonormal basis used by directional-light shadow cameras. It is derived
  // from light direction instead of Camera's usual world-up setup so overhead
  // lights do not hit the same degeneracy as ordinary view cameras.
  struct DirectionalShadowBasis {
    Vector3d forward;
    Vector3d right;
    Vector3d up;
  };

  inline DirectionalShadowBasis directionalShadowBasis(const Vector3d& lightDirection) {
    DirectionalShadowBasis basis;
    basis.forward = (-lightDirection).normalized();
    const Vector3d upCandidate =
      std::abs(basis.forward * Vector3d::up()) > 0.95 ? Vector3d::forward() : Vector3d::up();
    basis.right = (upCandidate ^ basis.forward).normalized();
    basis.up = (basis.right ^ -basis.forward).normalized();
    return basis;
  }

  // Snap the light-space center to the shadow-map texel grid. This keeps small
  // camera edits from moving a cascade by fractional texels, which is the source
  // of the most obvious shadow shimmer in the Modeler preview.
  inline Vector3d stabilizeDirectionalShadowCenter(const Vector3d& center,
                                                   const Vector3d& lightDirection,
                                                   double halfExtent, int shadowMapSize) {
    if (center.isUndefined() || center.isInfinite() || !std::isfinite(halfExtent) ||
        halfExtent <= 0.0 || shadowMapSize <= 0)
      return center;

    const double texelSize = (halfExtent * 2.0) / static_cast<double>(shadowMapSize);
    if (!std::isfinite(texelSize) || texelSize <= 0.0)
      return center;

    const DirectionalShadowBasis basis = directionalShadowBasis(lightDirection);
    if (basis.right.isUndefined() || basis.up.isUndefined())
      return center;

    const auto snap = [texelSize](double coordinate) {
      return std::round(coordinate / texelSize) * texelSize;
    };
    const double x = center * basis.right;
    const double y = center * basis.up;
    return center + basis.right * (snap(x) - x) + basis.up * (snap(y) - y);
  }

  // Orthographic camera used only for directional-light depth passes. It keeps
  // the Camera-shaped projection interface the raster emitter already consumes,
  // but does not support primary rays because no raytracing happens in this
  // pass.
  class DirectionalShadowCamera : public render::Camera {
  public:
    DirectionalShadowCamera(const Vector3d& center, const Vector3d& lightDirection,
                            double halfExtent)
        : m_halfExtent(halfExtent) {
      const DirectionalShadowBasis basis = directionalShadowBasis(lightDirection);
      m_forward = basis.forward;
      m_right = basis.right;
      m_up = basis.up;
      m_origin = center - m_forward * (halfExtent * 2.0);
    }

    Rayd rayForPixel(double, double, render::SampleStream&) const override {
      return Rayd::undefined;
    }

    std::shared_ptr<render::Camera> clone() const override {
      auto result = std::shared_ptr<DirectionalShadowCamera>(
        new DirectionalShadowCamera(m_origin, m_forward, m_right, m_up, m_halfExtent));
      copyBaseStateTo(*result);
      return result;
    }

    Vector3d projectPointWithDepth(const Vector3d& worldPoint) const override {
      const Vector3d cameraPoint = toCameraSpace(worldPoint);
      if (cameraPoint.z() < 0.0)
        return Vector3d::undefined;

      const auto plane = viewPlane();
      return Vector3d((cameraPoint.x() / m_halfExtent + 1.0) * plane->width() / 2.0,
                      (cameraPoint.y() / m_halfExtent + 1.0) * plane->height() / 2.0,
                      cameraPoint.z());
    }

    Vector4d projectPointToClipSpace(const Vector3d& worldPoint) const override {
      const Vector3d cameraPoint = toCameraSpace(worldPoint);
      return Vector4d(cameraPoint.x() / m_halfExtent, cameraPoint.y() / m_halfExtent,
                      cameraPoint.z(), 1.0);
    }

  private:
    DirectionalShadowCamera(const Vector3d& origin, const Vector3d& forward, const Vector3d& right,
                            const Vector3d& up, double halfExtent)
        : m_origin(origin),
          m_forward(forward),
          m_right(right),
          m_up(up),
          m_halfExtent(halfExtent) {
    }

    Vector3d toCameraSpace(const Vector3d& worldPoint) const {
      const Vector3d rel = worldPoint - m_origin;
      return Vector3d(rel * m_right, rel * m_up, rel * m_forward);
    }

    Vector3d m_origin;
    Vector3d m_forward;
    Vector3d m_right;
    Vector3d m_up;
    double m_halfExtent;
  };

  // One camera-depth slice in a cascaded directional shadow map. The camera
  // defines the light-space projection; the depth buffer stores nearest occluder
  // depth for that projection; the view-depth range chooses the cascade at
  // shading time.
  struct DirectionalShadowCascade {
    std::shared_ptr<DirectionalShadowCamera> camera;
    std::unique_ptr<Buffer<double>> depthBuffer;
    double minViewDepth;
    double maxViewDepth;
  };

  // Visibility query object for one directional light. It chooses a cascade
  // from camera-space depth, projects the shaded point into that cascade, and
  // applies the configured hard/PCF/PCSS depth comparison.
  class DirectionalShadowMap {
  public:
    DirectionalShadowMap(const render::Light* light, const render::Camera* viewCamera,
                         std::vector<DirectionalShadowCascade> cascades, double bias,
                         int filterRadius, Rasterizer::ShadowFilterMode filterMode)
        : m_light(light),
          m_viewCamera(viewCamera),
          m_cascades(std::move(cascades)),
          m_bias(bias),
          m_filterRadius(filterRadius),
          m_filterMode(filterMode) {
    }

    const render::Light* light() const {
      return m_light;
    }

    double visibility(const Vector3d& worldPos) const {
      const DirectionalShadowCascade* cascade = cascadeFor(worldPos);
      if (!cascade)
        return 1.0;

      const Vector3d shadowPixel = cascade->camera->projectPointWithDepth(worldPos);
      if (shadowPixel.isUndefined())
        return 1.0;

      const int x = static_cast<int>(std::lround(shadowPixel.x()));
      const int y = static_cast<int>(std::lround(shadowPixel.y()));

      if (m_filterRadius == 0)
        return sampleVisibility(*cascade, x, y, shadowPixel.z());

      if (m_filterMode == Rasterizer::ShadowFilterMode::PCSS)
        return pcssVisibility(*cascade, x, y, shadowPixel.z());

      return pcfVisibility(*cascade, x, y, shadowPixel.z(), m_filterRadius);
    }

  private:
    const DirectionalShadowCascade* cascadeFor(const Vector3d& worldPos) const {
      if (m_cascades.empty())
        return nullptr;

      if (m_cascades.size() == 1 || !m_viewCamera)
        return &m_cascades.front();

      const Vector3d viewPixel = m_viewCamera->projectPointWithDepth(worldPos);
      if (viewPixel.isUndefined())
        return &m_cascades.front();

      const double viewDepth = viewPixel.z();
      for (const auto& cascade : m_cascades) {
        if (viewDepth >= cascade.minViewDepth && viewDepth <= cascade.maxViewDepth)
          return &cascade;
      }

      return viewDepth < m_cascades.front().minViewDepth ? &m_cascades.front() : &m_cascades.back();
    }

    double pcfVisibility(const DirectionalShadowCascade& cascade, int x, int y,
                         double receiverDepth, int radius) const {
      double litSamples = 0.0;
      int samples = 0;
      for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
          litSamples += sampleVisibility(cascade, x + dx, y + dy, receiverDepth);
          ++samples;
        }
      }
      return litSamples / static_cast<double>(samples);
    }

    double pcssVisibility(const DirectionalShadowCascade& cascade, int x, int y,
                          double receiverDepth) const {
      double blockerDepthSum = 0.0;
      int blockerSamples = 0;
      for (int dy = -m_filterRadius; dy <= m_filterRadius; ++dy) {
        for (int dx = -m_filterRadius; dx <= m_filterRadius; ++dx) {
          const double blockerDepth = sampleBlockerDepth(cascade, x + dx, y + dy, receiverDepth);
          if (std::isfinite(blockerDepth)) {
            blockerDepthSum += blockerDepth;
            ++blockerSamples;
          }
        }
      }

      if (blockerSamples == 0)
        return 1.0;

      const double averageBlockerDepth = blockerDepthSum / static_cast<double>(blockerSamples);
      const int radius = pcssFilterRadius(receiverDepth, averageBlockerDepth);
      return pcfVisibility(cascade, x, y, receiverDepth, radius);
    }

    int pcssFilterRadius(double receiverDepth, double blockerDepth) const {
      const double gap = std::max(0.0, receiverDepth - blockerDepth - m_bias);
      return std::clamp(static_cast<int>(std::ceil(gap)), 1, m_filterRadius);
    }

    double sampleBlockerDepth(const DirectionalShadowCascade& cascade, int x, int y,
                              double receiverDepth) const {
      if (x < 0 || y < 0 || x >= cascade.depthBuffer->width() || y >= cascade.depthBuffer->height())
        return std::numeric_limits<double>::infinity();

      const double occluderDepth = (*cascade.depthBuffer)[y][x];
      if (!std::isfinite(occluderDepth))
        return std::numeric_limits<double>::infinity();

      return receiverDepth > occluderDepth + m_bias ? occluderDepth
                                                    : std::numeric_limits<double>::infinity();
    }

    double sampleVisibility(const DirectionalShadowCascade& cascade, int x, int y,
                            double receiverDepth) const {
      if (x < 0 || y < 0 || x >= cascade.depthBuffer->width() || y >= cascade.depthBuffer->height())
        return 1.0;

      const double occluderDepth = (*cascade.depthBuffer)[y][x];
      if (!std::isfinite(occluderDepth))
        return 1.0;

      return receiverDepth <= occluderDepth + m_bias ? 1.0 : 0.0;
    }

    const render::Light* m_light;
    const render::Camera* m_viewCamera;
    std::vector<DirectionalShadowCascade> m_cascades;
    double m_bias;
    int m_filterRadius;
    Rasterizer::ShadowFilterMode m_filterMode;
  };

  // Frame-level shadow-map collection. The built-in material evaluator asks it
  // for visibility per light; lights without a matching shadow map are treated
  // as fully visible so non-directional lights and disabled shadow passes keep
  // their direct contribution.
  class ShadowMaps {
  public:
    void add(DirectionalShadowMap shadowMap) {
      m_directional.push_back(std::move(shadowMap));
    }

    bool empty() const {
      return m_directional.empty();
    }

    double visibility(const render::Light* light, const Vector3d& worldPos) const {
      for (const auto& shadowMap : m_directional) {
        if (shadowMap.light() == light)
          return shadowMap.visibility(worldPos);
      }
      return 1.0;
    }

  private:
    std::vector<DirectionalShadowMap> m_directional;
  };

  // Compute the scene-bound depth interval visible to the main camera. Cascades
  // subdivide this interval; invalid or clipped-away bounds fall back to the
  // configured clip range so the shadow builder still has a finite range.
  inline std::pair<double, double> viewDepthRange(const render::Camera& camera,
                                                  const std::array<Vector3d, 8>& corners,
                                                  double nearClipDepth, double farClipDepth) {
    double minDepth = std::numeric_limits<double>::infinity();
    double maxDepth = 0.0;

    for (const Vector3d& corner : corners) {
      const double depth = camera.eyeRelativeDepth(corner);
      if (!std::isfinite(depth))
        continue;
      if (depth > nearClipDepth && depth < farClipDepth) {
        minDepth = std::min(minDepth, depth);
        maxDepth = std::max(maxDepth, depth);
      }
    }

    if (!std::isfinite(minDepth) || maxDepth <= minDepth) {
      const double fallbackMax =
        std::isfinite(farClipDepth) ? farClipDepth : std::max(nearClipDepth * 2.0, maxDepth);
      return {nearClipDepth, fallbackMax};
    }

    return {minDepth, maxDepth};
  }

  // Evenly split the camera-depth interval into cascade ranges. This is simple
  // linear splitting; the v2 plan tracks revisiting the split policy later.
  inline std::vector<std::pair<double, double>> cascadeDepthRanges(double minDepth, double maxDepth,
                                                                   int cascadeCount) {
    std::vector<std::pair<double, double>> ranges;
    ranges.reserve(static_cast<std::size_t>(cascadeCount));
    const double span = maxDepth - minDepth;
    for (int i = 0; i != cascadeCount; ++i) {
      const double start = minDepth + span * static_cast<double>(i) / cascadeCount;
      const double end = minDepth + span * static_cast<double>(i + 1) / cascadeCount;
      ranges.emplace_back(start, end);
    }
    return ranges;
  }

  // Include the point where a scene-bounds edge crosses a cascade split plane.
  // This lets cascade bounds include sliced faces rather than only original box
  // corners that happen to fall inside the depth range.
  inline void includeDepthPlaneIntersection(BoundingBoxd& bounds, const Vector3d& a, double depthA,
                                            const Vector3d& b, double depthB, double splitDepth) {
    if (!std::isfinite(depthA) || !std::isfinite(depthB) || depthA == depthB)
      return;

    const bool crosses =
      (depthA < splitDepth && depthB > splitDepth) || (depthB < splitDepth && depthA > splitDepth);
    if (!crosses)
      return;

    const double t = (splitDepth - depthA) / (depthB - depthA);
    bounds.include(a + (b - a) * t);
  }

  // Build a world-space box around the portion of the scene bounds that belongs
  // to one camera-depth cascade. The current implementation uses this as input
  // to a square light-space projection; tighter light-space fitting is a future
  // quality/performance follow-up.
  inline BoundingBoxd cascadeBoundsForDepthRange(const BoundingBoxd& sceneBounds,
                                                 const std::array<Vector3d, 8>& corners,
                                                 const render::Camera& camera, double minDepth,
                                                 double maxDepth) {
    static constexpr std::array<std::array<int, 2>, 12> edges = {{
      {{0, 1}},
      {{0, 2}},
      {{0, 4}},
      {{1, 3}},
      {{1, 5}},
      {{2, 3}},
      {{2, 6}},
      {{3, 7}},
      {{4, 5}},
      {{4, 6}},
      {{5, 7}},
      {{6, 7}},
    }};

    std::array<double, 8> depths{};
    for (std::size_t i = 0; i != corners.size(); ++i) {
      depths[i] = camera.eyeRelativeDepth(corners[i]);
    }

    BoundingBoxd result;
    for (std::size_t i = 0; i != corners.size(); ++i) {
      if (std::isfinite(depths[i]) && depths[i] >= minDepth && depths[i] <= maxDepth)
        result.include(corners[i]);
    }

    for (const auto& edge : edges) {
      const int a = edge[0];
      const int b = edge[1];
      includeDepthPlaneIntersection(result, corners[a], depths[a], corners[b], depths[b], minDepth);
      includeDepthPlaneIntersection(result, corners[a], depths[a], corners[b], depths[b], maxDepth);
    }

    return result.isValid() ? result : sceneBounds;
  }

}
