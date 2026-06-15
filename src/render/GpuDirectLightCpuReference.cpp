#include "render/GpuDirectLightCpuReference.h"

#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "render/GpuCompiledLightSampler.h"
#include "render/IntersectionService.h"
#include "render/MIS.h"
#include "render/State.h"
#include "render/samplers/GpuSampleStream.h"

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

    Vector2d gpuSample2D(const GpuDirectLightSampleStateRecord& sample, std::uint32_t dimension) {
      return GpuSampleStream::sample2D(sample.seed, sample.pixelIndex, sample.primarySampleIndex,
                                       dimension);
    }

    double gpuSample1D(const GpuDirectLightSampleStateRecord& sample, std::uint32_t dimension) {
      return GpuSampleStream::sample1D(GpuSampleCoordinate{sample.seed, sample.pixelIndex,
                                                           sample.primarySampleIndex, dimension,
                                                           /*component=*/0});
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
      Vector2d lightSample(0.5, 0.5);

      if (static_cast<GpuTracingLightKind>(light.kind) == GpuTracingLightKind::RectangularArea) {
        const std::uint32_t surfaceDimension =
          static_cast<std::uint32_t>(gpuDirectLightSurfaceSampleDimension(
            work.sample.bounce, lightIndex, work.sample.directSampleIndex));
        lightSample = gpuSample2D(work.sample, surfaceDimension);
      }

      const GpuCompiledLightSample sample = sampleGpuCompiledLight(light, point, lightSample);
      if (!sample.valid()) {
        return invalidVisibility(workIndex, lightIndex);
      }

      const double normalDotOut = normal * sample.direction;
      if (selectionPdf <= 0.0 || sample.radiance == Colord::black() || normalDotOut <= 0.0) {
        return invalidVisibility(workIndex, lightIndex);
      }

      const Rayd shadowRay(point4(work.surface.point), sample.direction);
      const Rayd shifted = shadowRay.epsilonShifted();

      GpuDirectLightVisibilityRecord visibility;
      visibility.workIndex = workIndex;
      visibility.lightIndex = lightIndex;
      visibility.flags =
        gpuDirectLightVisibilityValid | (sample.delta ? gpuDirectLightVisibilityDeltaLight : 0u);
      visibility.rayOrigin = vector4(Vector3d(shifted.origin()), 1.0f);
      visibility.rayDirection = vector4(shifted.direction(), 0.0f);
      visibility.lightRadiance = color4(sample.radiance);
      visibility.lightSample = {static_cast<float>(sample.surfaceSample.x()),
                                static_cast<float>(sample.surfaceSample.y()), 0.0f, 0.0f};
      visibility.minDistance = static_cast<float>(Rayd::epsilon);
      visibility.maxDistance = static_cast<float>(sample.distance);
      visibility.lightPdf = static_cast<float>(sample.pdf);
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

    Rayd visibilityRay(const GpuDirectLightVisibilityRecord& visibility) {
      return Rayd(Vector4d(visibility.rayOrigin[0], visibility.rayOrigin[1],
                           visibility.rayOrigin[2], visibility.rayOrigin[3]),
                  vector3(visibility.rayDirection));
    }

  }

  GpuDirectLightVisibilityRecord
  makeGpuDirectLightCpuVisibilityRecord(const GpuTracingSceneSections& scene,
                                        const GpuDirectLightWorkRecord& work,
                                        std::uint32_t workIndex) {
    const double selectionSample = gpuSample1D(work.sample, work.sample.lightSelectionDimension);
    const GpuCompiledLightSelection selection =
      selectGpuCompiledLight(scene, work.lightSelection, selectionSample);
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

  std::vector<GpuDirectLightVisibilityRecord> resolveGpuDirectLightCpuVisibilityOcclusionBatch(
    IntersectionService& intersectionService,
    const std::vector<GpuDirectLightVisibilityRecord>& visibility) {
    std::vector<GpuDirectLightVisibilityRecord> result = visibility;
    std::vector<State> states(result.size());
    std::vector<std::size_t> queryToVisibility;
    std::vector<WavefrontAnyHitQuery> queries;
    queryToVisibility.reserve(result.size());
    queries.reserve(result.size());

    for (std::size_t index = 0; index != result.size(); ++index) {
      result[index].occluded = 0u;
      if ((result[index].flags & gpuDirectLightVisibilityValid) == 0u) {
        continue;
      }
      queryToVisibility.push_back(index);
      queries.push_back(WavefrontAnyHitQuery{visibilityRay(result[index]),
                                             result[index].maxDistance, &states[index]});
    }

    if (queries.empty()) {
      return result;
    }

    const WavefrontOcclusionFlags occluded = intersectionService.anyHits(queries);
    if (occluded.size() != queries.size()) {
      throw std::logic_error("direct-light CPU reference visibility occlusion size mismatch");
    }
    for (std::size_t queryIndex = 0; queryIndex != occluded.size(); ++queryIndex) {
      result[queryToVisibility[queryIndex]].occluded = occluded[queryIndex] != 0U ? 1u : 0u;
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

  GpuDirectLightCpuReferenceBatch
  makeGpuDirectLightCpuReferenceBatch(const GpuTracingSceneSections& scene,
                                      const std::vector<GpuDirectLightWorkRecord>& work,
                                      IntersectionService& intersectionService) {
    GpuDirectLightCpuReferenceBatch result;
    result.visibility = resolveGpuDirectLightCpuVisibilityOcclusionBatch(
      intersectionService, makeGpuDirectLightCpuVisibilityBatch(scene, work));
    result.contributions = makeGpuDirectLightCpuContributionBatch(scene, work, result.visibility);
    return result;
  }
}
