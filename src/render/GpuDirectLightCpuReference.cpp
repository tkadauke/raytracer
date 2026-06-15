#include "render/GpuDirectLightCpuReference.h"

#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "render/MIS.h"
#include "render/samplers/GpuSampleStream.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace render {
  namespace {
    constexpr double tolerance = 1e-9;

    static_assert(std::is_standard_layout_v<GpuDirectLightContributionRecord>);
    static_assert(alignof(GpuDirectLightContributionRecord) == 16);
    static_assert(sizeof(GpuDirectLightContributionRecord) % 16 == 0);

    Vector3d vector3(const std::array<float, 4>& value) {
      return Vector3d(value[0], value[1], value[2]);
    }

    Vector4d point4(const std::array<float, 4>& value) {
      return Vector4d(value[0], value[1], value[2], value[3]);
    }

    Colord color(const std::array<float, 4>& value) {
      return Colord(value[0], value[1], value[2]);
    }

    std::array<float, 4> vector4(const Vector3d& value, float w) {
      return {static_cast<float>(value.x()), static_cast<float>(value.y()),
              static_cast<float>(value.z()), w};
    }

    std::array<float, 4> color4(const Colord& value) {
      return {static_cast<float>(value.r()), static_cast<float>(value.g()),
              static_cast<float>(value.b()), 1.0f};
    }

    std::array<float, 4> zero4() {
      return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    double maxColor(const std::array<float, 4>& value) {
      return std::max({0.0f, value[0], value[1], value[2]});
    }

    double rectangleArea(const GpuTracingLightRecord& light) {
      return (vector3(light.u) ^ vector3(light.v)).length();
    }

    double lightSelectionWeight(const GpuTracingLightRecord& light) {
      const auto kind = static_cast<GpuTracingLightKind>(light.kind);
      switch (kind) {
      case GpuTracingLightKind::Point:
      case GpuTracingLightKind::Directional:
        return maxColor(light.parameters);
      case GpuTracingLightKind::RectangularArea:
        return maxColor(light.parameters) * rectangleArea(light) * PI;
      case GpuTracingLightKind::Unsupported:
        return 0.0;
      }
      return 0.0;
    }

    struct LightSelection {
      bool valid{false};
      std::uint32_t lightIndex{0};
      double pdf{0.0};
    };

    LightSelection selectLight(const GpuTracingSceneSections& scene,
                               const GpuDirectLightSelectionRecord& selection, double unitSample) {
      if (selection.lightCount == 0u || selection.lightBegin >= scene.lights.size()) {
        return {};
      }

      const std::uint32_t availableCount = static_cast<std::uint32_t>(
        std::min<std::size_t>(selection.lightCount, scene.lights.size() - selection.lightBegin));
      std::vector<double> weights(availableCount, 0.0);
      double totalWeight = 0.0;
      for (std::uint32_t offset = 0; offset != availableCount; ++offset) {
        weights[offset] = lightSelectionWeight(scene.lights[selection.lightBegin + offset]);
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

    Vector2d gpuSample2D(const GpuDirectLightSampleStateRecord& sample, std::uint32_t dimension) {
      return GpuSampleStream::sample2D(sample.seed, sample.pixelIndex, sample.primarySampleIndex,
                                       dimension);
    }

    double gpuSample1D(const GpuDirectLightSampleStateRecord& sample, std::uint32_t dimension) {
      return GpuSampleStream::sample1D(GpuSampleCoordinate{sample.seed, sample.pixelIndex,
                                                           sample.primarySampleIndex, dimension,
                                                           /*component=*/0});
    }

    Vector3d areaLightPoint(const GpuTracingLightRecord& light, const Vector2d& sample) {
      return vector3(light.positionOrDirection) + vector3(light.u) * (sample.x() - 0.5) +
             vector3(light.v) * (sample.y() - 0.5);
    }

    double areaLightSurfaceCosine(const GpuTracingLightRecord& light,
                                  const Vector3d& directionToLight) {
      const Vector3d normal = (vector3(light.u) ^ vector3(light.v)).normalizedOrZero(tolerance);
      return std::max(0.0, normal * -directionToLight);
    }

    GpuDirectLightVisibilityRecord invalidVisibility(std::uint32_t workIndex,
                                                     std::uint32_t lightIndex = 0) {
      GpuDirectLightVisibilityRecord visibility;
      visibility.workIndex = workIndex;
      visibility.lightIndex = lightIndex;
      visibility.lightRadiance = zero4();
      visibility.lightSample = zero4();
      return visibility;
    }

    GpuDirectLightVisibilityRecord makeVisibilityForLight(const GpuDirectLightWorkRecord& work,
                                                          std::uint32_t workIndex,
                                                          std::uint32_t lightIndex,
                                                          double selectionPdf,
                                                          const GpuTracingLightRecord& light) {
      const Vector3d point = vector3(work.surface.point);
      const Vector3d normal = vector3(work.surface.normal).normalizedOrZero(tolerance);
      Vector3d directionToLight = Vector3d::null;
      Colord radiance = Colord::black();
      double distance = std::numeric_limits<double>::infinity();
      double lightPdf = 0.0;
      bool delta = false;
      Vector2d lightSample(0.5, 0.5);

      const auto kind = static_cast<GpuTracingLightKind>(light.kind);
      switch (kind) {
      case GpuTracingLightKind::Point: {
        const Vector3d offset = vector3(light.positionOrDirection) - point;
        distance = offset.length();
        if (distance <= tolerance) {
          return invalidVisibility(workIndex, lightIndex);
        }
        directionToLight = offset / distance;
        radiance = color(light.parameters);
        lightPdf = 1.0;
        delta = true;
        break;
      }
      case GpuTracingLightKind::Directional:
        directionToLight = vector3(light.positionOrDirection).normalizedOrZero(tolerance);
        radiance = color(light.parameters);
        lightPdf = 1.0;
        delta = true;
        break;
      case GpuTracingLightKind::RectangularArea: {
        const std::uint32_t surfaceDimension =
          static_cast<std::uint32_t>(gpuDirectLightSurfaceSampleDimension(
            work.sample.bounce, lightIndex, work.sample.directSampleIndex));
        lightSample = gpuSample2D(work.sample, surfaceDimension);
        const double area = rectangleArea(light);
        if (area <= tolerance) {
          return invalidVisibility(workIndex, lightIndex);
        }

        const Vector3d offset = areaLightPoint(light, lightSample) - point;
        distance = offset.length();
        if (distance <= tolerance) {
          return invalidVisibility(workIndex, lightIndex);
        }
        directionToLight = offset / distance;
        const double cosLight = areaLightSurfaceCosine(light, directionToLight);
        if (cosLight <= tolerance) {
          return invalidVisibility(workIndex, lightIndex);
        }
        radiance = color(light.parameters);
        lightPdf = (distance * distance) / (cosLight * area);
        break;
      }
      case GpuTracingLightKind::Unsupported:
        return invalidVisibility(workIndex, lightIndex);
      }

      const double normalDotOut = normal * directionToLight;
      if (selectionPdf <= 0.0 || lightPdf <= 0.0 || radiance == Colord::black() ||
          normalDotOut <= 0.0) {
        return invalidVisibility(workIndex, lightIndex);
      }

      const Rayd shadowRay(point4(work.surface.point), directionToLight);
      const Rayd shifted = shadowRay.epsilonShifted();

      GpuDirectLightVisibilityRecord visibility;
      visibility.workIndex = workIndex;
      visibility.lightIndex = lightIndex;
      visibility.flags =
        gpuDirectLightVisibilityValid | (delta ? gpuDirectLightVisibilityDeltaLight : 0u);
      visibility.rayOrigin = vector4(Vector3d(shifted.origin()), 1.0f);
      visibility.rayDirection = vector4(shifted.direction(), 0.0f);
      visibility.lightRadiance = color4(radiance);
      visibility.lightSample = {static_cast<float>(lightSample.x()),
                                static_cast<float>(lightSample.y()), 0.0f, 0.0f};
      visibility.minDistance = static_cast<float>(Rayd::epsilon);
      visibility.maxDistance = static_cast<float>(distance);
      visibility.lightPdf = static_cast<float>(lightPdf);
      visibility.selectionPdf = static_cast<float>(selectionPdf);
      return visibility;
    }

    Colord textureColor(const GpuTracingSceneSections& scene, std::uint32_t textureIndex) {
      if (textureIndex >= scene.textures.size()) {
        return Colord::black();
      }
      const GpuTracingTextureRecord& texture = scene.textures[textureIndex];
      if (static_cast<GpuTracingTextureKind>(texture.kind) !=
          GpuTracingTextureKind::ConstantColor) {
        return Colord::black();
      }
      return color(texture.parameters);
    }

  }

  GpuDirectLightVisibilityRecord
  makeGpuDirectLightCpuVisibilityRecord(const GpuTracingSceneSections& scene,
                                        const GpuDirectLightWorkRecord& work,
                                        std::uint32_t workIndex) {
    const double selectionSample = gpuSample1D(work.sample, work.sample.lightSelectionDimension);
    const LightSelection selection = selectLight(scene, work.lightSelection, selectionSample);
    if (!selection.valid || selection.lightIndex >= scene.lights.size()) {
      return invalidVisibility(workIndex);
    }

    GpuDirectLightVisibilityRecord visibility = makeVisibilityForLight(
      work, workIndex, selection.lightIndex, selection.pdf, scene.lights[selection.lightIndex]);
    visibility.occluded = 0u;
    return visibility;
  }

  std::vector<GpuDirectLightVisibilityRecord>
  makeGpuDirectLightCpuVisibilityBatch(const GpuTracingSceneSections& scene,
                                       const std::vector<GpuDirectLightWorkRecord>& work) {
    std::vector<GpuDirectLightVisibilityRecord> result;
    result.reserve(work.size());
    for (std::size_t index = 0; index != work.size(); ++index) {
      result.push_back(makeGpuDirectLightCpuVisibilityRecord(scene, work[index],
                                                             static_cast<std::uint32_t>(index)));
    }
    return result;
  }

  GpuDirectLightContributionRecord
  makeGpuDirectLightCpuContributionRecord(const GpuTracingSceneSections& scene,
                                          const GpuDirectLightWorkRecord& work,
                                          const GpuDirectLightVisibilityRecord& visibility) {
    GpuDirectLightContributionRecord result;
    result.workIndex = visibility.workIndex;
    result.lightIndex = visibility.lightIndex;
    result.occluded = visibility.occluded;

    if ((visibility.flags & gpuDirectLightVisibilityValid) == 0u ||
        work.surface.material >= scene.materials.size()) {
      return result;
    }

    result.flags |= gpuDirectLightContributionValid;
    if (visibility.occluded != 0u) {
      result.flags |= gpuDirectLightContributionOccluded;
      return result;
    }

    const GpuTracingMaterialRecord& material = scene.materials[work.surface.material];
    if (static_cast<GpuTracingMaterialKind>(material.kind) != GpuTracingMaterialKind::Matte) {
      return result;
    }

    const Colord albedo = textureColor(scene, material.albedoTexture);
    const double diffuseCoefficient = material.parameters[1];
    const Colord bsdfValue = albedo * diffuseCoefficient * invPI;
    if (bsdfValue == Colord::black()) {
      return result;
    }

    const Vector3d wi = vector3(work.surface.incomingDirection).normalizedOrZero(tolerance);
    const Vector3d wo = vector3(visibility.rayDirection).normalizedOrZero(tolerance);
    const Vector3d normal = vector3(work.surface.normal).normalizedOrZero(tolerance);
    const double normalDotOut = normal * wo;
    double bsdfPdf = 0.0;
    if (normal * wi >= 0.0 && normalDotOut > 0.0) {
      bsdfPdf = normalDotOut * invPI;
    }

    const bool deltaLight = (visibility.flags & gpuDirectLightVisibilityDeltaLight) != 0u;
    const double otherPdf = deltaLight ? 0.0 : bsdfPdf;
    Colord contribution = mis::estimateDirectLightingFromLightSample(
      bsdfValue, color(visibility.lightRadiance), normalDotOut, visibility.lightPdf, otherPdf,
      deltaLight);
    if (visibility.selectionPdf > 0.0) {
      contribution = contribution / visibility.selectionPdf;
    } else {
      contribution = Colord::black();
    }
    contribution = contribution * color(work.surface.throughput);

    if (contribution != Colord::black()) {
      result.flags |= gpuDirectLightContributionContributing;
      result.contribution = color4(contribution);
    }
    return result;
  }

  std::vector<GpuDirectLightContributionRecord> makeGpuDirectLightCpuContributionBatch(
    const GpuTracingSceneSections& scene, const std::vector<GpuDirectLightWorkRecord>& work,
    const std::vector<GpuDirectLightVisibilityRecord>& visibility) {
    if (work.size() != visibility.size()) {
      throw std::logic_error("direct-light CPU reference contribution batch size mismatch");
    }

    std::vector<GpuDirectLightContributionRecord> result;
    result.reserve(work.size());
    for (std::size_t index = 0; index != work.size(); ++index) {
      result.push_back(
        makeGpuDirectLightCpuContributionRecord(scene, work[index], visibility[index]));
    }
    return result;
  }

  GpuDirectLightCpuReferenceBatch
  makeGpuDirectLightCpuReferenceBatch(const GpuTracingSceneSections& scene,
                                      const std::vector<GpuDirectLightWorkRecord>& work) {
    GpuDirectLightCpuReferenceBatch result;
    result.visibility = makeGpuDirectLightCpuVisibilityBatch(scene, work);
    result.contributions = makeGpuDirectLightCpuContributionBatch(scene, work, result.visibility);
    return result;
  }
}
