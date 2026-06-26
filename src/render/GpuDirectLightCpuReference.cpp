#include "render/GpuDirectLightCpuReference.h"

#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "render/GpuCompiledLightSampler.h"
#include "render/GpuTracingTextureEvaluator.h"
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
      visibility.lightRadiance = {};
      visibility.lightSample = {};
      return visibility;
    }

    GpuDirectLightVisibilityRecord makeVisibilityForLight(const GpuDirectLightWorkRecord& work,
                                                          std::uint32_t workIndex,
                                                          std::uint32_t lightIndex,
                                                          double selectionPdf,
                                                          const GpuTracingLightRecord& light) {
      const Vector3d point = Vector3d(work.surface.point);
      const Vector3d normal = Vector3d(work.surface.normal).normalizedOrZero(tolerance);
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

      const Rayd shadowRay(Vector4d(work.surface.point), sample.direction);
      const Rayd shifted = shadowRay.epsilonShifted();

      GpuDirectLightVisibilityRecord visibility;
      visibility.workIndex = workIndex;
      visibility.lightIndex = lightIndex;
      visibility.flags =
        gpuDirectLightVisibilityValid | (sample.delta ? gpuDirectLightVisibilityDeltaLight : 0u);
      visibility.rayOrigin = Vector3d(shifted.origin()).toFloat4(1.0f);
      visibility.rayDirection = shifted.direction().toFloat4();
      visibility.lightRadiance = sample.radiance.toFloat4();
      visibility.lightSample = {static_cast<float>(sample.surfaceSample.x()),
                                static_cast<float>(sample.surfaceSample.y()), 0.0f, 0.0f};
      visibility.minDistance = static_cast<float>(Rayd::epsilon);
      visibility.maxDistance = static_cast<float>(sample.distance);
      visibility.lightPdf = static_cast<float>(sample.pdf);
      visibility.selectionPdf = static_cast<float>(selectionPdf);
      return visibility;
    }

    Rayd visibilityRay(const GpuDirectLightVisibilityRecord& visibility) {
      return Rayd(Vector4d(visibility.rayOrigin[0], visibility.rayOrigin[1],
                           visibility.rayOrigin[2], visibility.rayOrigin[3]),
                  Vector3d(visibility.rayDirection));
    }

    GpuIntersectionHitRecord surfaceHitRecord(const GpuDirectLightSurfaceRecord& surface) {
      GpuIntersectionHitRecord hit;
      hit.material = surface.material;
      hit.object = surface.object;
      hit.primitiveRecord = surface.primitiveRecord;
      hit.rayIndex = surface.pathIndex;
      hit.point = surface.point;
      hit.normal = surface.normal;
      hit.uv = surface.uv;
      return hit;
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

    const std::size_t queryCount = queries.size();
    const WavefrontOcclusionFlags occluded =
      intersectionService.resolveDirectLightVisibility(std::move(queries));
    if (occluded.size() != queryCount) {
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

    const Colord albedo = GpuTracingTextureEvaluator(scene).evaluate(
      material.albedoTexture, surfaceHitRecord(work.surface));
    const double diffuseCoefficient = material.parameters[1];
    const Colord bsdfValue = albedo * diffuseCoefficient * invPI;
    if (bsdfValue == Colord::black()) {
      return result;
    }

    const Vector3d wi = Vector3d(work.surface.incomingDirection).normalizedOrZero(tolerance);
    const Vector3d wo = Vector3d(visibility.rayDirection).normalizedOrZero(tolerance);
    const Vector3d normal = Vector3d(work.surface.normal).normalizedOrZero(tolerance);
    const double normalDotOut = normal * wo;
    double bsdfPdf = 0.0;
    if (normal * wi >= 0.0 && normalDotOut > 0.0) {
      bsdfPdf = normalDotOut * invPI;
    }

    const bool deltaLight = (visibility.flags & gpuDirectLightVisibilityDeltaLight) != 0u;
    const double otherPdf = deltaLight ? 0.0 : bsdfPdf;
    Colord contribution = mis::estimateDirectLightingFromLightSample(
      bsdfValue, Colord(visibility.lightRadiance), normalDotOut, visibility.lightPdf, otherPdf,
      deltaLight);
    if (visibility.selectionPdf > 0.0) {
      contribution = contribution / visibility.selectionPdf;
    } else {
      contribution = Colord::black();
    }
    contribution = contribution * Colord(work.surface.throughput);

    if (contribution != Colord::black()) {
      result.flags |= gpuDirectLightContributionContributing;
      result.contribution = contribution.toFloat4();
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
