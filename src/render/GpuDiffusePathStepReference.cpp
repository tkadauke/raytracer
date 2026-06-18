#include "render/GpuDiffusePathStepReference.h"

#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "render/MIS.h"
#include "render/PathTermination.h"
#include "render/samplers/GpuSampleStream.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>

using namespace render;

namespace {
  constexpr std::uint32_t kBsdfSampleDimensionOffset = 0;
  constexpr std::uint32_t kLightSampleDimensionOffset = 1;
  constexpr std::uint32_t kLightSelectionDimensionOffset = 2;
  constexpr std::uint32_t kContinuationDimensionOffset = 3;
  constexpr double kLightTolerance = 1e-9;
  constexpr const char* kPackedCpuExecutionPath = "packed_cpu";
  constexpr const char* kCpuRecordExecutionPath = "cpu_record";

  Rayd rayFromRecord(const GpuIntersectionRay& ray) {
    return Rayd(Vector4d(ray.origin), Vector3d(ray.direction));
  }

  GpuIntersectionRay packRay(const Rayd& ray, std::uint32_t rayIndex, double minDistance,
                             double maxDistance) {
    return GpuIntersectionScenePacker().packRay(ray, rayIndex, minDistance, maxDistance,
                                                /*timeSample=*/0.0);
  }

  std::uint32_t sampleDimension(const GpuDiffusePathStateRecord& pathState, std::uint32_t offset) {
    return pathState.sampleDimensionBase + pathState.depth * pathState.sampleDimensionStride +
           offset;
  }

  double sample1D(const GpuDiffusePathStateRecord& pathState, std::uint32_t dimension) {
    return GpuSampleStream::sample1D(GpuSampleCoordinate{pathState.sampleSeed, pathState.pixelIndex,
                                                         pathState.primarySampleIndex, dimension,
                                                         /*component=*/0});
  }

  Vector2d sample2D(const GpuDiffusePathStateRecord& pathState, std::uint32_t dimension) {
    return GpuSampleStream::sample2D(pathState.sampleSeed, pathState.pixelIndex,
                                     pathState.primarySampleIndex, dimension);
  }

  Colord textureColor(const GpuTracingSceneSections& scene, std::uint32_t textureId) {
    if (textureId >= scene.textures.size()) {
      return Colord::black();
    }

    const GpuTracingTextureRecord& texture = scene.textures[textureId];
    if (texture.kind != static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor)) {
      return Colord::black();
    }

    return Colord(texture.parameters);
  }

  Colord environmentRadiance(const GpuTracingSceneSections& scene) {
    return scene.environment.empty() ? Colord::black() : Colord(scene.environment.front().color);
  }

  struct LightSampleRecord {
    bool valid{false};
    Vector3d direction;
    Colord radiance{Colord::black()};
    double distance{std::numeric_limits<double>::infinity()};
    double pdf{0.0};
    bool delta{false};
  };

  double rectangularLightSurfaceCosine(const GpuTracingLightRecord& light,
                                       const Vector3d& directionToLight) {
    const Vector3d normal =
      (Vector3d(light.u) ^ Vector3d(light.v)).normalizedOrZero(kLightTolerance);
    return std::max(0.0, normal * -directionToLight);
  }

  Vector3d rectangularLightPoint(const GpuTracingLightRecord& light, const Vector2d& sample) {
    return Vector3d(light.positionOrDirection) + Vector3d(light.u) * (sample.x() - 0.5) +
           Vector3d(light.v) * (sample.y() - 0.5);
  }

  double rectangularLightArea(const GpuTracingLightRecord& light) {
    return (Vector3d(light.u) ^ Vector3d(light.v)).length();
  }

  LightSampleRecord sampleLight(const GpuTracingLightRecord& light, const Vector3d& point,
                                const Vector2d& sample) {
    const auto kind = static_cast<GpuTracingLightKind>(light.kind);
    if (kind == GpuTracingLightKind::Point) {
      const Vector3d offset = Vector3d(light.positionOrDirection) - point;
      const double distance = offset.length();
      if (distance <= kLightTolerance) {
        return {};
      }
      return {true, offset / distance, Colord(light.parameters), distance, 1.0, true};
    }

    if (kind == GpuTracingLightKind::Directional) {
      return {true,
              Vector3d(light.positionOrDirection).normalized(),
              Colord(light.parameters),
              std::numeric_limits<double>::infinity(),
              1.0,
              true};
    }

    if (kind == GpuTracingLightKind::RectangularArea) {
      const double area = rectangularLightArea(light);
      if (area <= kLightTolerance) {
        return {};
      }

      const Vector3d lightPoint = rectangularLightPoint(light, sample);
      const Vector3d offset = lightPoint - point;
      const double distance = offset.length();
      if (distance <= kLightTolerance) {
        return {};
      }

      const Vector3d direction = offset / distance;
      const double cosLight = rectangularLightSurfaceCosine(light, direction);
      if (cosLight <= kLightTolerance) {
        return {};
      }

      return {true,
              direction,
              Colord(light.parameters),
              distance,
              (distance * distance) / (cosLight * area),
              false};
    }

    return {};
  }

  double lightPdf(const GpuTracingSceneSections& scene, const Vector3d& point,
                  const Vector3d& direction) {
    if (scene.lights.empty()) {
      return 0.0;
    }

    double pdf = 0.0;
    for (const GpuTracingLightRecord& light : scene.lights) {
      const auto kind = static_cast<GpuTracingLightKind>(light.kind);
      if (kind == GpuTracingLightKind::Point || kind == GpuTracingLightKind::Directional) {
        continue;
      }

      if (kind == GpuTracingLightKind::RectangularArea) {
        const Vector3d normal =
          (Vector3d(light.u) ^ Vector3d(light.v)).normalizedOrZero(kLightTolerance);
        const double normalDotDirection = normal * direction;
        if (std::abs(normalDotDirection) <= kLightTolerance) {
          continue;
        }
        const double t =
          ((Vector3d(light.positionOrDirection) - point) * normal) / normalDotDirection;
        if (t <= kLightTolerance) {
          continue;
        }

        const Vector3d lightPoint = point + direction * t;
        const Vector3d local = lightPoint - Vector3d(light.positionOrDirection);
        const Vector3d u = Vector3d(light.u);
        const Vector3d v = Vector3d(light.v);
        const double uu = u * u;
        const double uv = u * v;
        const double vv = v * v;
        const double lu = local * u;
        const double lv = local * v;
        const double determinant = uu * vv - uv * uv;
        if (std::abs(determinant) <= kLightTolerance) {
          continue;
        }
        const double localU = (vv * lu - uv * lv) / determinant;
        const double localV = (uu * lv - uv * lu) / determinant;
        if (localU < -0.5 - kLightTolerance || localU > 0.5 + kLightTolerance ||
            localV < -0.5 - kLightTolerance || localV > 0.5 + kLightTolerance) {
          continue;
        }

        const double cosLight = rectangularLightSurfaceCosine(light, direction.normalized());
        if (cosLight > kLightTolerance) {
          pdf += (t * t) / (cosLight * rectangularLightArea(light));
        }
      }
    }

    return pdf / static_cast<double>(scene.lights.size());
  }

  Vector3d tangentFor(const Vector3d& normal) {
    const Vector3d helper = std::abs(normal.y()) < 0.999 ? Vector3d::up() : Vector3d::right();
    return (helper ^ normal).normalized();
  }

  Vector3d cosineHemisphereDirection(const Vector3d& normal, const Vector2d& sample) {
    const double u0 = std::clamp(sample.x(), 0.0, 1.0);
    const double u1 = std::clamp(sample.y(), 0.0, 1.0);
    const double r = std::sqrt(u0);
    const double phi = TAU * u1;
    const double x = r * std::cos(phi);
    const double y = r * std::sin(phi);
    const double z = std::sqrt(std::max(0.0, 1.0 - u0));
    const Vector3d tangent = tangentFor(normal);
    const Vector3d bitangent = normal ^ tangent;
    return (tangent * x + bitangent * y + normal * z).normalized();
  }

  double cosineHemispherePdf(const Vector3d& normal, const Vector3d& direction) {
    const double normalDotDirection = normal * direction;
    return normalDotDirection <= 0.0 ? 0.0 : normalDotDirection * invPI;
  }

  Colord matteBsdf(const GpuTracingSceneSections& scene, const GpuTracingMaterialRecord& material) {
    return textureColor(scene, material.albedoTexture) * material.parameters[1] * invPI;
  }

  void terminate(GpuDiffusePathStateRecord& pathState) {
    pathState.flags &= ~gpuDiffusePathStateActiveFlag;
    pathState.flags |= gpuDiffusePathStateTerminatedFlag;
  }

  void markUnsupported(GpuDiffusePathStateRecord& pathState) {
    pathState.flags |= gpuDiffusePathStateUnsupportedFlag;
    terminate(pathState);
  }

  std::map<std::uint32_t, GpuIntersectionHitRecord>
  hitsByRayIndex(const std::vector<GpuIntersectionHitRecord>& closestHits) {
    std::map<std::uint32_t, GpuIntersectionHitRecord> result;
    for (const GpuIntersectionHitRecord& hit : closestHits) {
      result[hit.rayIndex] = hit;
    }
    return result;
  }
}

GpuDiffusePathStepResult
GpuDiffusePathStep::step(const GpuTracingSceneSections& scene,
                         const std::vector<GpuDiffusePathStateRecord>& pathStates) const {
  std::vector<GpuIntersectionRay> activeRays;
  activeRays.reserve(pathStates.size());
  for (const GpuDiffusePathStateRecord& pathState : pathStates) {
    if (gpuDiffusePathStateIsActive(pathState)) {
      activeRays.push_back(pathState.ray);
    }
  }

  const std::vector<GpuIntersectionHitRecord> closestHits =
    GpuIntersectionIntersector().intersectClosest(scene.geometry, activeRays);
  GpuDiffusePathStepResult result =
    GpuDiffusePathStepReference().step(scene, pathStates, closestHits);
  result.closestHitRecords = closestHits;
  if (!activeRays.empty()) {
    result.metrics.closestHitExecutionPath = kPackedCpuExecutionPath;
    result.metrics.closestHitRays = activeRays.size();
  }
  return result;
}

GpuDiffusePathStepResult
GpuDiffusePathStepReference::step(const GpuTracingSceneSections& scene,
                                  const std::vector<GpuDiffusePathStateRecord>& pathStates,
                                  const std::vector<GpuIntersectionHitRecord>& closestHits) const {
  GpuDiffusePathStepResult result;
  result.closestHitRecords = closestHits;
  result.stepRecords.resize(pathStates.size());
  result.pathStates.reserve(pathStates.size());

  const std::map<std::uint32_t, GpuIntersectionHitRecord> hitRecords = hitsByRayIndex(closestHits);
  GpuIntersectionIntersector intersector;

  for (std::size_t pathIndex = 0; pathIndex != pathStates.size(); ++pathIndex) {
    GpuDiffusePathStateRecord pathState = pathStates[pathIndex];
    GpuDiffusePathStepRecord& stepRecord = result.stepRecords[pathIndex];
    stepRecord.pathIndex = static_cast<std::uint32_t>(pathIndex);
    stepRecord.pixelIndex = pathState.pixelIndex;
    stepRecord.primarySampleIndex = pathState.primarySampleIndex;
    stepRecord.depth = pathState.depth;

    if (!gpuDiffusePathStateIsActive(pathState)) {
      continue;
    }

    ++result.metrics.activePaths;
    const auto hitIt = hitRecords.find(pathState.ray.rayIndex);
    const GpuIntersectionHitRecord hit =
      hitIt == hitRecords.end() ? GpuIntersectionScenePacker().packMiss(pathState.ray.rayIndex)
                                : hitIt->second;

    if (!hit.hit) {
      const Colord contribution = Colord(pathState.throughput) * environmentRadiance(scene);
      pathState.accumulatedRadiance =
        (Colord(pathState.accumulatedRadiance) + contribution).toFloat4(0.0f);
      stepRecord.event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss);
      stepRecord.missRadiance = contribution.toFloat4(0.0f);
      terminate(pathState);
      stepRecord.flags = pathState.flags;
      ++result.metrics.misses;
      ++result.metrics.terminatedPaths;
      continue;
    }

    ++result.metrics.hits;
    stepRecord.event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit);
    stepRecord.material = hit.material;
    stepRecord.object = hit.object;

    if (hit.material >= scene.materials.size()) {
      markUnsupported(pathState);
      stepRecord.event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Unsupported);
      stepRecord.flags = pathState.flags;
      ++result.metrics.unsupportedHits;
      ++result.metrics.terminatedPaths;
      continue;
    }

    const GpuTracingMaterialRecord& material = scene.materials[hit.material];
    const auto materialKind = static_cast<GpuTracingMaterialKind>(material.kind);
    const Vector3d point = Vector3d(hit.point);
    const Vector3d normal = Vector3d(hit.normal).normalized();
    const Rayd ray = rayFromRecord(pathState.ray);
    const Vector3d wi = -ray.direction().normalized();
    const Colord throughput = Colord(pathState.throughput);
    Colord accumulated = Colord(pathState.accumulatedRadiance);

    if (materialKind == GpuTracingMaterialKind::Emissive) {
      result.metrics.emissionExecutionPath = kCpuRecordExecutionPath;
      ++result.metrics.emissionContributionEvaluations;
      const Colord emitted =
        (normal * wi) > 0.0 ? textureColor(scene, material.emissionTexture) : Colord::black();
      const Colord contribution = throughput * emitted;
      accumulated += contribution;
      pathState.accumulatedRadiance = accumulated.toFloat4(0.0f);
      stepRecord.emittedRadiance = contribution.toFloat4(0.0f);
      terminate(pathState);
      stepRecord.flags = pathState.flags;
      ++result.metrics.emissiveHits;
      ++result.metrics.terminatedPaths;
      continue;
    }

    if (materialKind != GpuTracingMaterialKind::Matte) {
      markUnsupported(pathState);
      stepRecord.event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Unsupported);
      stepRecord.flags = pathState.flags;
      ++result.metrics.unsupportedHits;
      ++result.metrics.terminatedPaths;
      continue;
    }

    const Colord bsdf = matteBsdf(scene, material);
    if (!scene.lights.empty()) {
      const double selectionSample =
        sample1D(pathState, sampleDimension(pathState, kLightSelectionDimensionOffset));
      std::uint32_t lightIndex =
        std::min(static_cast<std::uint32_t>(selectionSample * scene.lights.size()),
                 static_cast<std::uint32_t>(scene.lights.size() - 1));
      const double selectionPdf = 1.0 / static_cast<double>(scene.lights.size());
      const LightSampleRecord light =
        sampleLight(scene.lights[lightIndex], point,
                    sample2D(pathState, sampleDimension(pathState, kLightSampleDimensionOffset)));
      if (light.valid) {
        result.metrics.directLightVisibilityExecutionPath = kPackedCpuExecutionPath;
        result.metrics.directLightContributionExecutionPath = kCpuRecordExecutionPath;
        ++result.metrics.directLightSamples;
        const Rayd shadowRay = Rayd(Vector4d(hit.point), light.direction).epsilonShifted();
        const std::uint32_t shadowRayIndex =
          static_cast<std::uint32_t>(result.directLightShadowRays.size());
        const GpuIntersectionRay packedShadowRay =
          packRay(shadowRay, shadowRayIndex, /*minDistance=*/0.0, light.distance);
        const bool occluded = intersector.intersectAny(scene.geometry, packedShadowRay);
        result.directLightShadowRays.push_back(packedShadowRay);
        result.directLightOcclusionRecords.push_back(
          GpuIntersectionOcclusionRecord{occluded ? 1u : 0u, shadowRayIndex, {}});
        ++result.metrics.directLightVisibilityRays;
        if (occluded) {
          ++result.metrics.directLightOccludedSamples;
        } else {
          ++result.metrics.directLightContributionEvaluations;
          const double bsdfPdf = light.delta ? 0.0 : cosineHemispherePdf(normal, light.direction);
          const Colord lightContribution =
            mis::estimateDirectLightingFromLightSample(
              bsdf, light.radiance, normal * light.direction, light.pdf, bsdfPdf, light.delta) /
            selectionPdf;
          const Colord contribution = throughput * lightContribution;
          if (contribution != Colord::black()) {
            ++result.metrics.directLightContributingSamples;
          }
          accumulated += contribution;
          stepRecord.directLightRadiance = contribution.toFloat4(0.0f);
        }
      }
    }

    const Vector2d bsdfSample =
      sample2D(pathState, sampleDimension(pathState, kBsdfSampleDimensionOffset));
    const Vector3d wo = cosineHemisphereDirection(normal, bsdfSample);
    const double pdf = cosineHemispherePdf(normal, wo);
    const double normalDotOut = normal * wo;
    Colord nextThroughput =
      pdf <= 0.0 ? Colord::black() : throughput * (bsdf * (normalDotOut / pdf));
    const double roulette =
      sample1D(pathState, sampleDimension(pathState, kContinuationDimensionOffset));
    const PathContinuation continuation = pathContinuation(nextThroughput, roulette);
    nextThroughput = continuedThroughput(nextThroughput, continuation);

    if (nextThroughput == Colord::black()) {
      pathState.accumulatedRadiance = accumulated.toFloat4(0.0f);
      terminate(pathState);
      stepRecord.flags = pathState.flags;
      ++result.metrics.terminatedPaths;
      continue;
    }

    pathState.ray = packRay(Rayd(Vector4d(hit.point), wo).epsilonShifted(), pathState.ray.rayIndex,
                            /*minDistance=*/0.0, std::numeric_limits<double>::infinity());
    pathState.throughput = nextThroughput.toFloat4(0.0f);
    pathState.accumulatedRadiance = accumulated.toFloat4(0.0f);
    pathState.depth += 1u;
    pathState.previousBsdfPdf = static_cast<float>(pdf);
    pathState.previousLightPdf = static_cast<float>(lightPdf(scene, point, wo));
    pathState.previousMaterial = hit.material;
    pathState.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;
    pathState.flags |= gpuDiffusePathStateActiveFlag;
    pathState.flags &= ~gpuDiffusePathStateTerminatedFlag;
    stepRecord.continuationThroughput = nextThroughput.toFloat4(0.0f);
    stepRecord.flags = pathState.flags;
    result.pathStates.push_back(pathState);
    ++result.metrics.spawnedContinuations;
  }

  return result;
}
