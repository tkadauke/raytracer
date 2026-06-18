#include "render/GpuCompiledLightSampler.h"

#include "core/math/Constants.h"
#include "render/GpuFloat4.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace render {
  namespace {
    constexpr double tolerance = 1e-9;

    double rectangleArea(const GpuTracingLightRecord& light) {
      return (gpuFloat4ToVector3(light.u) ^ gpuFloat4ToVector3(light.v)).length();
    }

    Vector3d rectangleNormal(const GpuTracingLightRecord& light) {
      return (gpuFloat4ToVector3(light.u) ^ gpuFloat4ToVector3(light.v))
        .normalizedOrZero(tolerance);
    }

    Vector3d areaLightPoint(const GpuTracingLightRecord& light, const Vector2d& sample) {
      return gpuFloat4ToVector3(light.positionOrDirection) +
             gpuFloat4ToVector3(light.u) * (sample.x() - 0.5) +
             gpuFloat4ToVector3(light.v) * (sample.y() - 0.5);
    }

    double areaLightSurfaceCosine(const GpuTracingLightRecord& light,
                                  const Vector3d& directionToLight) {
      return std::max(0.0, rectangleNormal(light) * -directionToLight);
    }

    bool areaLightContainsPoint(const GpuTracingLightRecord& light, const Vector3d& point) {
      const Vector3d edgeU = gpuFloat4ToVector3(light.u);
      const Vector3d edgeV = gpuFloat4ToVector3(light.v);
      const Vector3d local = point - gpuFloat4ToVector3(light.positionOrDirection);
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
      return gpuFloat4MaxColor(light.parameters) * rectangleArea(light) * PI;
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
      const Vector3d offset = gpuFloat4ToVector3(light.positionOrDirection) - point;
      const double distance = offset.length();
      if (distance <= tolerance) {
        return invalidSample(GpuCompiledLightSampleStatus::CoincidentPoint, lightSample);
      }

      return {GpuCompiledLightSampleStatus::Valid,
              offset / distance,
              gpuFloat4ToColor(light.parameters),
              distance,
              1.0,
              true,
              lightSample};
    }
    case GpuTracingLightKind::Directional: {
      const Vector3d direction =
        gpuFloat4ToVector3(light.positionOrDirection).normalizedOrZero(tolerance);
      if (direction == Vector3d::null) {
        return invalidSample(GpuCompiledLightSampleStatus::DegenerateLight, lightSample);
      }

      return {GpuCompiledLightSampleStatus::Valid,
              direction,
              gpuFloat4ToColor(light.parameters),
              std::numeric_limits<double>::infinity(),
              1.0,
              true,
              lightSample};
    }
    case GpuTracingLightKind::RectangularArea: {
      const double area = rectangleArea(light);
      if (area <= tolerance) {
        return invalidSample(GpuCompiledLightSampleStatus::DegenerateLight, lightSample);
      }

      const Vector3d offset = areaLightPoint(light, lightSample) - point;
      const double distance = offset.length();
      if (distance <= tolerance) {
        return invalidSample(GpuCompiledLightSampleStatus::CoincidentPoint, lightSample);
      }

      const Vector3d directionToLight = offset / distance;
      const double cosLight = areaLightSurfaceCosine(light, directionToLight);
      if (cosLight <= tolerance) {
        return invalidSample(GpuCompiledLightSampleStatus::BackFacing, lightSample);
      }

      const double solidAnglePdf = (distance * distance) / (cosLight * area);
      return {GpuCompiledLightSampleStatus::Valid,
              directionToLight,
              gpuFloat4ToColor(light.parameters),
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
    if (rectangleArea(light) <= tolerance) {
      return 0.0;
    }

    const Vector3d normal = rectangleNormal(light);
    const double normalDotDirection = normal * direction;
    if (std::abs(normalDotDirection) <= tolerance) {
      return 0.0;
    }

    const double t =
      ((gpuFloat4ToVector3(light.positionOrDirection) - point) * normal) / normalDotDirection;
    if (t <= tolerance) {
      return 0.0;
    }

    const Vector3d lightPoint = point + direction * t;
    if (!areaLightContainsPoint(light, lightPoint)) {
      return 0.0;
    }

    const double cosLight = areaLightSurfaceCosine(light, direction.normalized());
    if (cosLight <= tolerance) {
      return 0.0;
    }

    return (t * t) / (cosLight * rectangleArea(light));
  }
}
