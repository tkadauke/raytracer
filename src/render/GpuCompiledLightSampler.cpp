#include "render/GpuCompiledLightSampler.h"

#include "core/math/Constants.h"
#include "render/GpuFloat4.h"
#include "render/GpuRectangularLightHelpers.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace render {
  namespace {
    constexpr double tolerance = 1e-9;

    bool areaLightContainsPoint(const GpuTracingLightRecord& light, const Vector3d& point) {
      const Vector3d edgeU = Vector3d(light.u);
      const Vector3d edgeV = Vector3d(light.v);
      const Vector3d local = point - Vector3d(light.positionOrDirection);
      const double uu = edgeU * edgeU;
      const double uv = edgeU * edgeV;
      const double vv = edgeV * edgeV;
      const double lu = local * edgeU;
      const double lv = local * edgeV;
      const double determinant = uu * vv - uv * uv;
      if (std::abs(determinant) <= tolerance) {
        return false;
      }

      const double u = (vv * lu - uv * lv) / determinant;
      const double v = (uu * lv - uv * lu) / determinant;
      return u >= -0.5 - tolerance && u <= 0.5 + tolerance && v >= -0.5 - tolerance &&
             v <= 0.5 + tolerance;
    }

    GpuCompiledLightSample invalidSample(GpuCompiledLightSampleStatus status,
                                         const Vector2d& lightSample = Vector2d(0.5, 0.5)) {
      GpuCompiledLightSample sample;
      sample.status = status;
      sample.surfaceSample = lightSample;
      return sample;
    }
  }

  double gpuCompiledLightSelectionWeight(const GpuTracingLightRecord& light) {
    const auto kind = static_cast<GpuTracingLightKind>(light.kind);
    switch (kind) {
    case GpuTracingLightKind::Point:
    case GpuTracingLightKind::Directional:
      return gpuFloat4MaxColor(light.parameters);
    case GpuTracingLightKind::RectangularArea:
      return gpuFloat4MaxColor(light.parameters) * rectangularLightArea(light) * PI;
    case GpuTracingLightKind::Unsupported:
      return 0.0;
    }
    return 0.0;
  }

  GpuCompiledLightSelection selectGpuCompiledLight(const GpuTracingSceneSections& scene,
                                                   const GpuDirectLightSelectionRecord& selection,
                                                   double unitSample) {
    if (selection.lightCount == 0u || selection.lightBegin >= scene.lights.size()) {
      return {};
    }

    const std::uint32_t availableCount = static_cast<std::uint32_t>(
      std::min<std::size_t>(selection.lightCount, scene.lights.size() - selection.lightBegin));
    std::vector<double> weights(availableCount, 0.0);
    double totalWeight = 0.0;
    for (std::uint32_t offset = 0; offset != availableCount; ++offset) {
      weights[offset] =
        gpuCompiledLightSelectionWeight(scene.lights[selection.lightBegin + offset]);
      totalWeight += weights[offset];
    }
    if (totalWeight <= 0.0) {
      std::fill(weights.begin(), weights.end(), 1.0);
      totalWeight = static_cast<double>(weights.size());
    }

    if (!(unitSample >= 0.0)) {
      unitSample = 0.0;
    }
    unitSample = std::min(unitSample, std::nextafter(1.0, 0.0));
    const double target = unitSample * totalWeight;
    double cumulative = 0.0;
    for (std::uint32_t offset = 0; offset != availableCount; ++offset) {
      cumulative += weights[offset];
      if (target < cumulative) {
        return {true, selection.lightBegin + offset, weights[offset] / totalWeight};
      }
    }

    const std::uint32_t last = availableCount - 1u;
    return {true, selection.lightBegin + last, weights[last] / totalWeight};
  }

  GpuCompiledLightSample sampleGpuCompiledLight(const GpuTracingLightRecord& light,
                                                const Vector3d& point,
                                                const Vector2d& lightSample) {
    const auto kind = static_cast<GpuTracingLightKind>(light.kind);
    switch (kind) {
    case GpuTracingLightKind::Point: {
      const Vector3d offset = Vector3d(light.positionOrDirection) - point;
      const double distance = offset.length();
      if (distance <= tolerance) {
        return invalidSample(GpuCompiledLightSampleStatus::CoincidentPoint, lightSample);
      }

      return {GpuCompiledLightSampleStatus::Valid,
              offset / distance,
              Colord(light.parameters),
              distance,
              1.0,
              true,
              lightSample};
    }
    case GpuTracingLightKind::Directional: {
      const Vector3d direction = Vector3d(light.positionOrDirection).normalizedOrZero(tolerance);
      if (direction == Vector3d::null) {
        return invalidSample(GpuCompiledLightSampleStatus::DegenerateLight, lightSample);
      }

      return {GpuCompiledLightSampleStatus::Valid,
              direction,
              Colord(light.parameters),
              std::numeric_limits<double>::infinity(),
              1.0,
              true,
              lightSample};
    }
    case GpuTracingLightKind::RectangularArea: {
      const double area = rectangularLightArea(light);
      if (area <= tolerance) {
        return invalidSample(GpuCompiledLightSampleStatus::DegenerateLight, lightSample);
      }

      const Vector3d offset = rectangularLightPoint(light, lightSample) - point;
      const double distance = offset.length();
      if (distance <= tolerance) {
        return invalidSample(GpuCompiledLightSampleStatus::CoincidentPoint, lightSample);
      }

      const Vector3d directionToLight = offset / distance;
      const double cosLight = rectangularLightSurfaceCosine(light, directionToLight, tolerance);
      if (cosLight <= tolerance) {
        return invalidSample(GpuCompiledLightSampleStatus::BackFacing, lightSample);
      }

      const double solidAnglePdf = (distance * distance) / (cosLight * area);
      return {GpuCompiledLightSampleStatus::Valid,
              directionToLight,
              Colord(light.parameters),
              distance,
              solidAnglePdf,
              false,
              lightSample};
    }
    case GpuTracingLightKind::Unsupported:
      return invalidSample(GpuCompiledLightSampleStatus::UnsupportedLight, lightSample);
    }
    return invalidSample(GpuCompiledLightSampleStatus::UnsupportedLight, lightSample);
  }

  double gpuCompiledLightPdf(const GpuTracingLightRecord& light, const Vector3d& point,
                             const Vector3d& direction) {
    const auto kind = static_cast<GpuTracingLightKind>(light.kind);
    if (kind != GpuTracingLightKind::RectangularArea) {
      return 0.0;
    }
    if (rectangularLightArea(light) <= tolerance) {
      return 0.0;
    }

    const Vector3d normal = rectangularLightNormal(light, tolerance);
    const double normalDotDirection = normal * direction;
    if (std::abs(normalDotDirection) <= tolerance) {
      return 0.0;
    }

    const double t = ((Vector3d(light.positionOrDirection) - point) * normal) / normalDotDirection;
    if (t <= tolerance) {
      return 0.0;
    }

    const Vector3d lightPoint = point + direction * t;
    if (!areaLightContainsPoint(light, lightPoint)) {
      return 0.0;
    }

    const double cosLight = rectangularLightSurfaceCosine(light, direction.normalized(), tolerance);
    if (cosLight <= tolerance) {
      return 0.0;
    }

    return (t * t) / (cosLight * rectangularLightArea(light));
  }
}
