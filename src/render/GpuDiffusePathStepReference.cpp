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
  constexpr std::uint32_t kLightSelectionDimensionOffset = 0;
  constexpr std::uint32_t kLightSampleDimensionOffset = 1;
  constexpr std::uint32_t kBsdfSampleDimensionOffset = 2;
  constexpr std::uint32_t kContinuationDimensionOffset = 3;
  constexpr double kLightTolerance = 1e-9;

  std::array<float, 4> color4(const Colord& color, float w = 0.0f) {
    return {static_cast<float>(color.r()), static_cast<float>(color.g()),
            static_cast<float>(color.b()), w};
  }

  Colord colorFrom4(const std::array<float, 4>& value) {
    return Colord(value[0], value[1], value[2]);
  }

  Vector3d vectorFrom4(const std::array<float, 4>& value) {
    return Vector3d(value[0], value[1], value[2]);
  }

  Vector4d pointFrom4(const std::array<float, 4>& value) {
    return Vector4d(value[0], value[1], value[2], value[3]);
  }

  Rayd rayFromRecord(const GpuIntersectionRay& ray) {
    return Rayd(pointFrom4(ray.origin), vectorFrom4(ray.direction));
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

    return colorFrom4(texture.parameters);
  }

  Colord environmentRadiance(const GpuTracingSceneSections& scene) {
    return scene.environment.empty() ? Colord::black()
                                     : colorFrom4(scene.environment.front().color);
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
      (vectorFrom4(light.u) ^ vectorFrom4(light.v)).normalizedOrZero(kLightTolerance);
    return std::max(0.0, normal * -directionToLight);
  }

  Vector3d rectangularLightPoint(const GpuTracingLightRecord& light, const Vector2d& sample) {
    return vectorFrom4(light.positionOrDirection) + vectorFrom4(light.u) * (sample.x() - 0.5) +
           vectorFrom4(light.v) * (sample.y() - 0.5);
  }

  double rectangularLightArea(const GpuTracingLightRecord& light) {
    return (vectorFrom4(light.u) ^ vectorFrom4(light.v)).length();
  }

  LightSampleRecord sampleLight(const GpuTracingLightRecord& light, const Vector3d& point,
                                const Vector2d& sample) {
    const auto kind = static_cast<GpuTracingLightKind>(light.kind);
    if (kind == GpuTracingLightKind::Point) {
      const Vector3d offset = vectorFrom4(light.positionOrDirection) - point;
      const double distance = offset.length();
      if (distance <= kLightTolerance) {
        return {};
      }
      return {true, offset / distance, colorFrom4(light.parameters), distance, 1.0, true};
    }

    if (kind == GpuTracingLightKind::Directional) {
      return {true,
              vectorFrom4(light.positionOrDirection).normalized(),
              colorFrom4(light.parameters),
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
              colorFrom4(light.parameters),
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
          (vectorFrom4(light.u) ^ vectorFrom4(light.v)).normalizedOrZero(kLightTolerance);
        const double normalDotDirection = normal * direction;
        if (std::abs(normalDotDirection) <= kLightTolerance) {
          continue;
        }
        const double t =
          ((vectorFrom4(light.positionOrDirection) - point) * normal) / normalDotDirection;
        if (t <= kLightTolerance) {
          continue;
        }

        const Vector3d lightPoint = point + direction * t;
        const Vector3d local = lightPoint - vectorFrom4(light.positionOrDirection);
        const Vector3d u = vectorFrom4(light.u);
        const Vector3d v = vectorFrom4(light.v);
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
  return result;
}

GpuDiffusePathStepResult
GpuDiffusePathStepReference::step(const GpuTracingSceneSections& scene,
                                  const std::vector<GpuDiffusePathStateRecord>& pathStates,
                                  const std::vector<GpuIntersectionHitRecord>& closestHits) const {
  GpuDiffusePathStepResult result;
  result.closestHitRecords = closestHits;
  result.pathStates = pathStates;
  result.stepRecords.resize(pathStates.size());

  const std::map<std::uint32_t, GpuIntersectionHitRecord> hitRecords = hitsByRayIndex(closestHits);
  GpuIntersectionIntersector intersector;

  for (std::size_t pathIndex = 0; pathIndex != result.pathStates.size(); ++pathIndex) {
    GpuDiffusePathStateRecord& pathState = result.pathStates[pathIndex];
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
      const Colord contribution = colorFrom4(pathState.throughput) * environmentRadiance(scene);
      pathState.accumulatedRadiance =
        color4(colorFrom4(pathState.accumulatedRadiance) + contribution);
      stepRecord.event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss);
      stepRecord.missRadiance = color4(contribution);
      terminate(pathState);
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
    const Vector3d point = vectorFrom4(hit.point);
    const Vector3d normal = vectorFrom4(hit.normal).normalized();
    const Rayd ray = rayFromRecord(pathState.ray);
    const Vector3d wi = -ray.direction().normalized();
    const Colord throughput = colorFrom4(pathState.throughput);
    Colord accumulated = colorFrom4(pathState.accumulatedRadiance);

    if (materialKind == GpuTracingMaterialKind::Emissive) {
      const Colord emitted =
        (normal * wi) > 0.0 ? textureColor(scene, material.emissionTexture) : Colord::black();
      const Colord contribution = throughput * emitted;
      accumulated += contribution;
      pathState.accumulatedRadiance = color4(accumulated);
      stepRecord.emittedRadiance = color4(contribution);
      terminate(pathState);
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
        ++result.metrics.directLightSamples;
        const Rayd shadowRay = Rayd(pointFrom4(hit.point), light.direction).epsilonShifted();
        const std::uint32_t shadowRayIndex =
          static_cast<std::uint32_t>(result.directLightShadowRays.size());
        const GpuIntersectionRay packedShadowRay =
          packRay(shadowRay, shadowRayIndex, /*minDistance=*/0.0, light.distance);
        const bool occluded = intersector.intersectAny(scene.geometry, packedShadowRay);
        result.directLightShadowRays.push_back(packedShadowRay);
        result.directLightOcclusionRecords.push_back(
          GpuIntersectionOcclusionRecord{occluded ? 1u : 0u, shadowRayIndex, {}});
        if (occluded) {
          ++result.metrics.directLightOccludedSamples;
        } else {
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
          stepRecord.directLightRadiance = color4(contribution);
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
      pathState.accumulatedRadiance = color4(accumulated);
      terminate(pathState);
      ++result.metrics.terminatedPaths;
      continue;
    }

    pathState.ray =
      packRay(Rayd(pointFrom4(hit.point), wo).epsilonShifted(), pathState.ray.rayIndex,
              /*minDistance=*/0.0, std::numeric_limits<double>::infinity());
    pathState.throughput = color4(nextThroughput);
    pathState.accumulatedRadiance = color4(accumulated);
    pathState.depth += 1u;
    pathState.previousBsdfPdf = static_cast<float>(pdf);
    pathState.previousLightPdf = static_cast<float>(lightPdf(scene, point, wo));
    pathState.previousMaterial = hit.material;
    pathState.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;
    pathState.flags |= gpuDiffusePathStateActiveFlag;
    pathState.flags &= ~gpuDiffusePathStateTerminatedFlag;
    stepRecord.continuationThroughput = color4(nextThroughput);
    stepRecord.flags = pathState.flags;
    ++result.metrics.spawnedContinuations;
  }

  return result;
}
