#include "render/GpuDiffusePathStepReference.h"

#include "core/Buffer.h"
#include "core/math/Constants.h"
#include "core/math/Ray.h"
#include "render/MIS.h"
#include "render/PathTermination.h"
#include "render/TracingAccumulationReference.h"
#include "render/cameras/Camera.h"
#include "render/samplers/GpuSampleStream.h"
#include "render/samplers/Sampler.h"
#include "render/tonemap/Tonemap.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>

using namespace render;

namespace {
  constexpr std::uint32_t kBsdfSampleDimensionOffset = 0;
  constexpr std::uint32_t kContinuationDimensionOffset = 3;
  constexpr double kLightTolerance = 1e-9;
  const double kPackedRayMinimumDistance = Ray<float>::epsilon;
  constexpr const char* kPackedCpuExecutionPath = "packed_cpu";
  constexpr const char* kCpuRecordExecutionPath = "cpu_record";
  constexpr const char* kFullGpuPathLoopExecutionPath = "full_gpu_subset";

  constexpr const char* compiledDiffusePathLoopGpuFallbackReason() {
    return "platform full-GPU path-loop kernel is not available yet";
  }

  constexpr const char* compiledDiffusePathLoopPathStateResidencyFallbackReason() {
    return "compiled CPU-reference path loop keeps path state on the host";
  }

  constexpr const char* compiledDiffusePathLoopFrontierCompactionFallbackReason() {
    return "compiled CPU-reference path loop compacts path state on the host";
  }

  constexpr const char* compiledDiffusePathLoopDirectLightSamplingFallbackReason() {
    return "compiled CPU-reference path loop samples direct lights on the host";
  }

  constexpr const char* compiledDiffusePathLoopDirectLightVisibilityFallbackReason() {
    return "compiled CPU-reference path loop creates and consumes direct-light visibility "
           "batches on the host";
  }

  constexpr const char* compiledDiffusePathLoopDirectLightContributionFallbackReason() {
    return "compiled CPU-reference path loop evaluates direct-light contribution on the host";
  }

  constexpr const char* compiledDiffusePathLoopResidentDirectLightUnavailableReason() {
    return "compiled CPU-reference path loop resolves direct-light visibility on the host";
  }

  constexpr const char* compiledDiffusePathLoopBsdfEvalFallbackReason() {
    return "compiled CPU-reference path loop evaluates diffuse BSDFs on the host";
  }

  constexpr const char* compiledDiffusePathLoopBsdfSampleFallbackReason() {
    return "compiled CPU-reference path loop samples diffuse BSDF continuations on the host";
  }

  constexpr const char* compiledDiffusePathLoopSpawnedContinuationsFallbackReason() {
    return "compiled CPU-reference path loop spawns path continuations on the host";
  }

  constexpr const char* compiledDiffusePathLoopSampleAccumulationFallbackReason() {
    return "compiled CPU-reference path loop accumulates samples with CPU reference storage";
  }

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

  std::uint32_t narrowDimension(std::uint64_t dimension) {
    return static_cast<std::uint32_t>(dimension & std::numeric_limits<std::uint32_t>::max());
  }

  std::uint32_t directLightSelectionDimension(const GpuDiffusePathStateRecord& pathState,
                                              std::uint32_t directSampleIndex) {
    return narrowDimension(sampleDimensionIndex(
      SampleDimension::LightSelection,
      SampleStream::lightSelectionSampleIndex(pathState.depth, directSampleIndex)));
  }

  std::uint32_t directLightSurfaceDimension(const GpuDiffusePathStateRecord& pathState,
                                            std::uint32_t lightIndex,
                                            std::uint32_t directSampleIndex) {
    return narrowDimension(sampleDimensionIndex(
      SampleDimension::Light,
      SampleStream::lightSampleIndex(pathState.depth, lightIndex, directSampleIndex)));
  }

  std::uint32_t directLightSampleCount(const GpuDiffusePathLoopSettings& settings) {
    return std::max(1u, settings.directLightSamples);
  }

  std::uint64_t saturatedPathStateBytes(std::uint64_t count) {
    constexpr std::uint64_t bytesPerPath = sizeof(GpuDiffusePathStateRecord);
    constexpr std::uint64_t maxValue = std::numeric_limits<std::uint64_t>::max();
    if (bytesPerPath != 0u && count > maxValue / bytesPerPath) {
      return maxValue;
    }
    return count * bytesPerPath;
  }

  std::uint64_t saturatedAdd(std::uint64_t first, std::uint64_t second) {
    constexpr std::uint64_t maxValue = std::numeric_limits<std::uint64_t>::max();
    if (second > maxValue - first) {
      return maxValue;
    }
    return first + second;
  }

  double fraction(std::uint64_t numerator, std::uint64_t denominator) {
    if (denominator == 0u) {
      return 0.0;
    }
    return static_cast<double>(numerator) / static_cast<double>(denominator);
  }

  bool executionPathUsesGpu(const std::string& path) {
    return path == kFullGpuPathLoopExecutionPath || path.find("metal") != std::string::npos ||
           path.find("vulkan") != std::string::npos;
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

  Vector2d textureCoordinates(const GpuTracingTextureRecord& texture,
                              const GpuIntersectionHitRecord& hit) {
    const auto mapping =
      static_cast<GpuTracingTextureMappingKind>(texture.flags & gpuTracingTextureMappingMask);
    if (mapping == GpuTracingTextureMappingKind::UV) {
      return Vector2d(hit.uv[0] * texture.parameters[0], hit.uv[1] * texture.parameters[1]);
    }
    if (mapping == GpuTracingTextureMappingKind::Planar) {
      return Vector2d(hit.point[0], hit.point[2]);
    }
    return Vector2d::null;
  }

  double normalizedTextureCoordinate(const GpuTracingTextureRecord& texture, double coordinate) {
    if ((texture.flags & gpuTracingTextureWrapClampFlag) != 0u) {
      return std::clamp(coordinate, 0.0, 1.0);
    }
    return coordinate - std::floor(coordinate);
  }

  int imageTextureCoordinate(const GpuTracingTextureRecord& texture, double coordinate, int size) {
    const int result =
      static_cast<int>(std::floor(normalizedTextureCoordinate(texture, coordinate) * size));
    if ((texture.flags & gpuTracingTextureWrapClampFlag) != 0u) {
      return std::clamp(result, 0, size - 1);
    }
    int wrapped = result % size;
    if (wrapped < 0) {
      wrapped += size;
    }
    return wrapped;
  }

  Colord textureColor(const GpuTracingSceneSections& scene, std::uint32_t textureId,
                      const GpuIntersectionHitRecord& hit, std::uint32_t depth = 0) {
    constexpr std::uint32_t maxTextureEvaluationDepth = 8;
    if (textureId >= scene.textures.size()) {
      return Colord::black();
    }
    if (depth >= maxTextureEvaluationDepth) {
      return Colord::black();
    }

    const GpuTracingTextureRecord& texture = scene.textures[textureId];
    const auto kind = static_cast<GpuTracingTextureKind>(texture.kind);
    if (kind == GpuTracingTextureKind::ConstantColor) {
      return Colord(texture.parameters);
    }

    if (kind == GpuTracingTextureKind::CheckerBoard) {
      const Vector2d st = textureCoordinates(texture, hit);
      const int parity =
        static_cast<int>(std::floor(st.x())) + static_cast<int>(std::floor(st.y()));
      const std::uint32_t childTexture =
        parity % 2 == 0 ? texture.payloadOffset : texture.payloadCount;
      return textureColor(scene, childTexture, hit, depth + 1);
    }

    if (kind == GpuTracingTextureKind::Image) {
      const int width = static_cast<int>(std::round(texture.parameters[2]));
      const int height = static_cast<int>(std::round(texture.parameters[3]));
      const std::uint64_t texelCount =
        width > 0 && height > 0
          ? static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height)
          : 0u;
      if (width <= 0 || height <= 0 || texture.payloadOffset >= scene.textures.size() ||
          texture.payloadCount != texelCount ||
          static_cast<std::uint64_t>(texture.payloadOffset) + texture.payloadCount >
            scene.textures.size()) {
        return Colord::black();
      }
      const Vector2d st = textureCoordinates(texture, hit);
      const int x = imageTextureCoordinate(texture, st.x(), width);
      const int y = imageTextureCoordinate(texture, st.y(), height);
      const std::uint32_t texelTexture =
        texture.payloadOffset + static_cast<std::uint32_t>(y * width + x);
      return textureColor(scene, texelTexture, hit, depth + 1);
    }

    return Colord::black();
  }

  Colord environmentRecordColor(const GpuTracingSceneSections& scene, std::size_t index) {
    return index < scene.environment.size() ? Colord(scene.environment[index].color)
                                            : Colord::black();
  }

  Colord visibleBackgroundRadiance(const GpuTracingSceneSections& scene) {
    return environmentRecordColor(scene, 0);
  }

  Colord environmentRadiance(const GpuTracingSceneSections& scene) {
    return scene.environment.empty() ? Colord::black()
                                     : environmentRecordColor(scene, scene.environment.size() - 1u);
  }

  Colord missRadiance(const GpuTracingSceneSections& scene,
                      const GpuDiffusePathStateRecord& pathState) {
    return pathState.depth == 0u ? visibleBackgroundRadiance(scene) : environmentRadiance(scene);
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

  bool isDiffusePathLoopMaterial(GpuTracingMaterialKind kind) {
    return kind == GpuTracingMaterialKind::Matte || kind == GpuTracingMaterialKind::Phong;
  }

  Colord diffuseBsdf(const GpuTracingSceneSections& scene, const GpuTracingMaterialRecord& material,
                     const GpuIntersectionHitRecord& hit) {
    return textureColor(scene, material.albedoTexture, hit) * material.parameters[1] * invPI;
  }

  void terminate(GpuDiffusePathStateRecord& pathState) {
    pathState.flags &= ~gpuDiffusePathStateActiveFlag;
    pathState.flags |= gpuDiffusePathStateTerminatedFlag;
  }

  void markUnsupported(GpuDiffusePathStateRecord& pathState) {
    pathState.flags |= gpuDiffusePathStateUnsupportedFlag;
    terminate(pathState);
  }

  void mergeLabel(std::string& target, const std::string& source) {
    if (source.empty()) {
      return;
    }
    if (target.empty()) {
      target = source;
      return;
    }
    if (target != source) {
      target = "mixed";
    }
  }

  void validateRetainedPathIndices(std::size_t inputPathCount,
                                   const std::vector<std::uint32_t>& retainedPathIndices) {
    std::uint32_t previous = 0;
    bool hasPrevious = false;
    for (const std::uint32_t index : retainedPathIndices) {
      if (index >= inputPathCount) {
        throw std::out_of_range(
          "gpu diffuse path frontier compaction retained path index is out of range");
      }
      if (hasPrevious && index <= previous) {
        throw std::invalid_argument(
          "gpu diffuse path frontier compaction retained path indices must be strictly "
          "increasing");
      }
      previous = index;
      hasPrevious = true;
    }
  }

  std::uint64_t movedRetainedPathCount(const std::vector<std::uint32_t>& retainedPathIndices) {
    std::uint64_t moved = 0;
    for (std::size_t index = 0; index != retainedPathIndices.size(); ++index) {
      if (retainedPathIndices[index] != index) {
        ++moved;
      }
    }
    return moved;
  }

  std::map<std::uint32_t, GpuIntersectionHitRecord>
  hitsByRayIndex(const std::vector<GpuIntersectionHitRecord>& closestHits,
                 const std::vector<GpuDiffusePathStateRecord>& pathStates) {
    std::map<std::uint32_t, bool> expectedRayIndices;
    for (const GpuDiffusePathStateRecord& pathState : pathStates) {
      if (!gpuDiffusePathStateIsActive(pathState)) {
        continue;
      }
      const auto [_, inserted] = expectedRayIndices.emplace(pathState.ray.rayIndex, false);
      if (!inserted) {
        throw std::logic_error("gpu diffuse path step active ray indices must be unique");
      }
    }

    if (closestHits.size() != expectedRayIndices.size()) {
      throw std::logic_error(
        "gpu diffuse path step closest-hit record count must match active path count");
    }

    std::map<std::uint32_t, GpuIntersectionHitRecord> result;
    for (const GpuIntersectionHitRecord& hit : closestHits) {
      const auto expected = expectedRayIndices.find(hit.rayIndex);
      if (expected == expectedRayIndices.end()) {
        throw std::logic_error(
          "gpu diffuse path step closest-hit record ray index does not match an active path");
      }

      const auto [_, inserted] = result.emplace(hit.rayIndex, hit);
      if (!inserted) {
        throw std::logic_error("gpu diffuse path step closest-hit ray indices must be unique");
      }
    }
    return result;
  }

  TracingAccumulationDiagnostics
  accumulateGpuDiffusePathLoopImage(const std::vector<GpuDiffusePathStateRecord>& records,
                                    const TracingAccumulationLayout& layout,
                                    TracingAccumulationBuffer& accumulation) {
    TracingAccumulationDiagnostics diagnostics = TracingAccumulationDiagnostics::forLayout(
      layout, "gpu_diffuse_path_loop", "resident_accumulation_resolve");
    diagnostics.recordClear();

    const std::uint64_t pixelCount = layout.pixelCount();
    for (const GpuDiffusePathStateRecord& record : records) {
      if (record.pixelIndex >= pixelCount) {
        throw std::out_of_range("gpu diffuse path-loop resolve pixel index is out of range");
      }
      const int x = static_cast<int>(record.pixelIndex % static_cast<std::uint64_t>(layout.width));
      const int y = static_cast<int>(record.pixelIndex / static_cast<std::uint64_t>(layout.width));
      accumulation.addSample(x, y, Colord(record.accumulatedRadiance));
      diagnostics.recordAdd(1);
    }

    return diagnostics;
  }

  std::string platformAccumulationBackend(const GpuDiffusePathLoopResult& result) {
    return result.platformAccumulationBackend.empty() ? "gpu_diffuse_path_loop"
                                                      : result.platformAccumulationBackend;
  }

  std::string platformAccumulationResidency(const GpuDiffusePathLoopResult& result) {
    return result.platformAccumulationResidency.empty() ? "platform_accumulation_buffer"
                                                        : result.platformAccumulationResidency;
  }

  bool platformAccumulationMatchesLayout(const GpuDiffusePathLoopResult& result,
                                         const TracingAccumulationLayout& layout) {
    if (!result.hasPlatformAccumulation()) {
      return false;
    }
    if (result.platformAccumulationColorSums.size() !=
        result.platformAccumulationSampleCounts.size()) {
      throw std::logic_error(
        "gpu diffuse path-loop platform accumulation plane sizes do not match");
    }
    return result.platformAccumulationColorSums.size() == layout.pixelCount();
  }

  Colord resolvedPlatformAccumulationColor(const GpuDiffusePathLoopResult& result,
                                           std::uint64_t pixelIndex) {
    const std::uint32_t count = result.platformAccumulationSampleCounts[pixelIndex];
    if (count == 0u) {
      return Colord::black();
    }
    return Colord(result.platformAccumulationColorSums[pixelIndex]) *
           (1.0 / static_cast<double>(count));
  }

  TracingAccumulationDiagnostics
  platformAccumulationDiagnostics(const GpuDiffusePathLoopResult& result,
                                  const TracingAccumulationLayout& layout) {
    TracingAccumulationDiagnostics diagnostics =
      TracingAccumulationDiagnostics::forLayout(layout, platformAccumulationBackend(result).c_str(),
                                                platformAccumulationResidency(result).c_str());
    diagnostics.recordClear();

    std::uint64_t addOperations = 0;
    std::uint64_t addedSamples = 0;
    for (const std::uint32_t count : result.platformAccumulationSampleCounts) {
      if (count == 0u) {
        continue;
      }
      ++addOperations;
      addedSamples += count;
    }
    diagnostics.recordAdd(addedSamples, addOperations);
    return diagnostics;
  }

  TracingAccumulationDiagnostics
  resolvePlatformAccumulationImage(const GpuDiffusePathLoopResult& result,
                                   const TracingAccumulationLayout& layout,
                                   Buffer<Colord>& target) {
    if (target.width() != layout.width || target.height() != layout.height) {
      throw std::invalid_argument(
        "gpu diffuse path-loop HDR target dimensions do not match layout");
    }

    TracingAccumulationDiagnostics diagnostics = platformAccumulationDiagnostics(result, layout);
    for (int y = 0; y != layout.height; ++y) {
      for (int x = 0; x != layout.width; ++x) {
        const std::uint64_t pixelIndex =
          static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(layout.width) +
          static_cast<std::uint64_t>(x);
        target[y][x] = resolvedPlatformAccumulationColor(result, pixelIndex);
      }
    }
    diagnostics.recordResolve();
    diagnostics.recordReadback(layout.accumulationBytes());
    return diagnostics;
  }

  TracingAccumulationDiagnostics
  resolvePlatformAccumulationImage(const GpuDiffusePathLoopResult& result,
                                   const TracingAccumulationLayout& layout,
                                   Buffer<unsigned int>& target, const Tonemap* tonemap) {
    if (target.width() != layout.width || target.height() != layout.height) {
      throw std::invalid_argument(
        "gpu diffuse path-loop display target dimensions do not match layout");
    }

    TracingAccumulationDiagnostics diagnostics = platformAccumulationDiagnostics(result, layout);
    for (int y = 0; y != layout.height; ++y) {
      for (int x = 0; x != layout.width; ++x) {
        const std::uint64_t pixelIndex =
          static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(layout.width) +
          static_cast<std::uint64_t>(x);
        const Colord color = resolvedPlatformAccumulationColor(result, pixelIndex);
        target[y][x] = (tonemap ? tonemap->apply(color) : color).rgb();
      }
    }
    diagnostics.recordResolve();
    diagnostics.recordReadback(layout.accumulationBytes());
    return diagnostics;
  }
}

GpuDiffusePrimaryPathStateGeneration
GpuDiffusePrimaryPathStateGenerator::generate(const Camera& camera, const Recti& rect,
                                              std::optional<std::uint64_t> tileSeed,
                                              std::uint32_t sampleSeed) const {
  GpuDiffusePrimaryPathStateGeneration result;
  result.requestedRect = rect;
  result.actualRect = camera.renderableRect(rect);
  if (result.actualRect.width() <= 0 || result.actualRect.height() <= 0) {
    return result;
  }

  auto plane = camera.viewPlane();
  if (!plane) {
    return result;
  }

  const auto sampler = plane->sampler();
  if (!sampler || sampler->numSamples() <= 0) {
    return result;
  }

  const std::size_t pixelCount = static_cast<std::size_t>(result.actualRect.width()) *
                                 static_cast<std::size_t>(result.actualRect.height());
  const std::size_t estimatedPathCount =
    pixelCount * static_cast<std::size_t>(sampler->numSamples());
  if (pixelCount != 0 &&
      estimatedPathCount / pixelCount != static_cast<std::size_t>(sampler->numSamples())) {
    throw std::overflow_error("gpu diffuse primary path-state count overflows");
  }
  if (estimatedPathCount > std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("gpu diffuse primary path-state count exceeds GPU ray index range");
  }

  result.pathStates.reserve(estimatedPathCount);
  SampleStreamStorage sampleStreams;
  sampleStreams.reserve(estimatedPathCount);
  const auto primaryRayGenerator = camera.primaryRayGenerator();
  const GpuIntersectionScenePacker packer;

  for (ViewPlane::Iterator pixel = plane->pixelBegin(result.actualRect),
                           end = plane->end(result.actualRect);
       pixel != end; ++pixel) {
    const std::uint64_t pixelHash = camera.primaryRayPixelHash(pixel, tileSeed);
    const std::uint32_t pixelIndex =
      static_cast<std::uint32_t>(static_cast<std::uint64_t>(pixel.row() - rect.top()) *
                                   static_cast<std::uint64_t>(rect.width()) +
                                 static_cast<std::uint64_t>(pixel.column() - rect.left()));

    for (int sampleIndex = 0; sampleIndex != sampler->numSamples(); ++sampleIndex) {
      SampleStream* stream = sampler->appendStream(sampleStreams, sampleIndex, pixelHash);
      const std::optional<Camera::PrimaryRay> primarySample =
        primaryRayGenerator->sample(pixel, *stream);
      if (!primarySample) {
        ++result.skippedPrimarySamples;
        continue;
      }

      GpuDiffusePathStateRecord pathState = makeActiveGpuDiffusePathState();
      pathState.ray = packer.packRay(
        primarySample->ray, static_cast<std::uint32_t>(result.pathStates.size()),
        /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), primarySample->timeSample);
      pathState.pixelIndex = pixelIndex;
      pathState.primarySampleIndex = static_cast<std::uint32_t>(sampleIndex);
      pathState.sampleSeed = sampleSeed;
      result.pathStates.push_back(pathState);
      ++result.generatedPrimarySamples;
    }
  }

  return result;
}

void GpuDiffusePathStepMetrics::merge(const GpuDiffusePathStepMetrics& source) {
  mergeLabel(closestHitExecutionPath, source.closestHitExecutionPath);
  mergeLabel(emissionExecutionPath, source.emissionExecutionPath);
  mergeLabel(directLightVisibilityExecutionPath, source.directLightVisibilityExecutionPath);
  mergeLabel(directLightContributionExecutionPath, source.directLightContributionExecutionPath);
  activePaths += source.activePaths;
  closestHitRays += source.closestHitRays;
  misses += source.misses;
  hits += source.hits;
  unsupportedHits += source.unsupportedHits;
  emissiveHits += source.emissiveHits;
  emissionContributionEvaluations += source.emissionContributionEvaluations;
  directLightSamples += source.directLightSamples;
  directLightVisibilityRays += source.directLightVisibilityRays;
  directLightContributionEvaluations += source.directLightContributionEvaluations;
  directLightContributingSamples += source.directLightContributingSamples;
  directLightOccludedSamples += source.directLightOccludedSamples;
  spawnedContinuations += source.spawnedContinuations;
  terminatedPaths += source.terminatedPaths;
}

std::uint64_t GpuDiffusePathFrontierCompactionResult::retainedPathCount() const {
  return retainedPathIndices.size();
}

std::uint64_t GpuDiffusePathFrontierCompactionResult::removedPathCount() const {
  return inputPathCount >= retainedPathCount() ? inputPathCount - retainedPathCount() : 0u;
}

std::uint64_t GpuDiffusePathFrontierCompactionResult::movedPathCount() const {
  return movedRetainedPathCount(retainedPathIndices);
}

std::uint64_t GpuDiffusePathFrontierCompactionResult::retainedIndexBytes() const {
  return retainedPathIndices.size() * sizeof(std::uint32_t);
}

const CpuReferenceGpuDiffusePathFrontierCompactionBackend&
CpuReferenceGpuDiffusePathFrontierCompactionBackend::instance() {
  static const CpuReferenceGpuDiffusePathFrontierCompactionBackend backend;
  return backend;
}

const char* CpuReferenceGpuDiffusePathFrontierCompactionBackend::name() const {
  return "cpu_diffuse_frontier_compaction";
}

const char* CpuReferenceGpuDiffusePathFrontierCompactionBackend::pathStateResidency() const {
  return "cpu_host";
}

GpuDiffusePathFrontierCompactionResult CpuReferenceGpuDiffusePathFrontierCompactionBackend::compact(
  const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
  const std::vector<std::uint32_t>& retainedPathIndices) const {
  validateRetainedPathIndices(sourceRecords.size(), retainedPathIndices);

  GpuDiffusePathFrontierCompactionResult result;
  result.executionPath = name();
  result.pathStateResidency = pathStateResidency();
  result.inputPathCount = sourceRecords.size();
  result.retainedPathIndices = retainedPathIndices;
  result.retainedRecords.reserve(retainedPathIndices.size());
  for (const std::uint32_t index : retainedPathIndices) {
    result.retainedRecords.push_back(sourceRecords[index]);
  }
  return result;
}

GpuDiffusePathStepResult
GpuDiffusePathStep::step(const GpuTracingSceneSections& scene,
                         const std::vector<GpuDiffusePathStateRecord>& pathStates,
                         const GpuDiffusePathLoopSettings& settings) const {
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
    GpuDiffusePathStepReference().step(scene, pathStates, closestHits, settings);
  result.closestHitRecords = closestHits;
  if (!activeRays.empty()) {
    result.metrics.closestHitExecutionPath = kPackedCpuExecutionPath;
    result.metrics.closestHitRays = activeRays.size();
  }
  return result;
}

std::uint64_t GpuDiffusePathLoopResult::pathStateBytesPerPath() const {
  return sizeof(GpuDiffusePathStateRecord);
}

std::uint64_t GpuDiffusePathLoopResult::residentPathStateBytes() const {
  return saturatedPathStateBytes(initialPathCount);
}

std::uint64_t GpuDiffusePathLoopResult::inputPathStateBytes() const {
  return saturatedPathStateBytes(inputPathCount());
}

std::uint64_t GpuDiffusePathLoopResult::retainedPathStateBytes() const {
  return saturatedPathStateBytes(retainedPathCount());
}

std::uint64_t GpuDiffusePathLoopResult::removedPathStateBytes() const {
  return saturatedPathStateBytes(removedPathCount());
}

std::uint64_t GpuDiffusePathLoopResult::retainedPathIndexBytes() const {
  return retainedIndexBytes;
}

std::uint64_t GpuDiffusePathLoopResult::compactionPassCount() const {
  return depthCount;
}

std::uint64_t GpuDiffusePathLoopResult::inputPathCount() const {
  return metrics.activePaths;
}

std::uint64_t GpuDiffusePathLoopResult::retainedPathCount() const {
  return metrics.spawnedContinuations;
}

std::uint64_t GpuDiffusePathLoopResult::removedPathCount() const {
  return static_cast<std::uint64_t>(resolvedPathStates.size());
}

std::uint64_t GpuDiffusePathLoopResult::movedPathCount() const {
  return retainedPathCount();
}

std::uint64_t GpuDiffusePathLoopResult::peakActivePathCount() const {
  if (activePathsPerDepth.empty()) {
    return 0;
  }
  return *std::max_element(activePathsPerDepth.begin(), activePathsPerDepth.end());
}

std::uint64_t GpuDiffusePathLoopResult::lastActivePathCount() const {
  return activePathsPerDepth.empty() ? 0 : activePathsPerDepth.back();
}

std::uint64_t GpuDiffusePathLoopResult::submittedIntersectionRayCount() const {
  return saturatedAdd(metrics.closestHitRays, metrics.directLightVisibilityRays);
}

bool GpuDiffusePathLoopResult::fullGpuPathLoopSupported() const {
  return executionPath == kFullGpuPathLoopExecutionPath;
}

bool GpuDiffusePathLoopResult::fullGpuPathLoopUnavailable() const {
  return !fullGpuPathLoopSupported();
}

bool GpuDiffusePathLoopResult::hasPlatformAccumulation() const {
  return !platformAccumulationColorSums.empty() || !platformAccumulationSampleCounts.empty();
}

std::string GpuDiffusePathLoopResult::platformLabel() const {
  if (!platformName.empty()) {
    return platformName;
  }
  return fullGpuPathLoopSupported() ? "platform_gpu_path_loop" : "none";
}

TracingExecutionCapabilityRecords GpuDiffusePathLoopResult::tracingCapabilities(
  const TracingAccumulationDiagnostics& accumulation) const {
  using Domain = TracingExecutionDomain;
  using Device = TracingExecutionDevice;

  const bool fullGpuLoop = fullGpuPathLoopSupported();
  const std::string platform = platformLabel();
  const std::string pathLoopExecution =
    executionPath.empty() ? "compiled_cpu_reference" : executionPath;

  auto fallback = [](Domain domain, std::string name, std::string path) {
    return TracingCapabilityRecord::fallbackRecord(domain, std::move(name), Device::GPU,
                                                   Device::CPU, std::move(path),
                                                   compiledDiffusePathLoopGpuFallbackReason());
  };
  auto fallbackWithReason = [](Domain domain, std::string name, std::string path,
                               const char* reason) {
    return TracingCapabilityRecord::fallbackRecord(domain, std::move(name), Device::GPU,
                                                   Device::CPU, std::move(path), reason);
  };
  auto restrictedCpuRecord = [](Domain domain, std::string name) {
    return TracingCapabilityRecord::restricted(
      domain, std::move(name), Device::CPU, "host_records",
      "compiled GPU-facing records are interpreted by the CPU reference path loop");
  };
  auto gpuRecord = [&](Domain domain, std::string name, std::string path) {
    return TracingCapabilityRecord::gpu(domain, std::move(name), platform, std::move(path));
  };

  const std::string closestHitPath = metrics.closestHitExecutionPath.empty()
                                       ? (fullGpuLoop ? pathLoopExecution : kPackedCpuExecutionPath)
                                       : metrics.closestHitExecutionPath;
  const std::string anyHitPath = metrics.directLightVisibilityExecutionPath.empty()
                                   ? (fullGpuLoop ? pathLoopExecution : kPackedCpuExecutionPath)
                                   : metrics.directLightVisibilityExecutionPath;
  const std::string directLightContributionPath =
    metrics.directLightContributionExecutionPath.empty()
      ? (fullGpuLoop ? pathLoopExecution : kCpuRecordExecutionPath)
      : metrics.directLightContributionExecutionPath;
  const std::string accumulationPath =
    accumulation.backend.empty() ? (fullGpuLoop ? pathLoopExecution : "gpu_diffuse_path_loop")
                                 : accumulation.backend;
  const std::string accumulationResidency =
    accumulation.residency.empty() ? (fullGpuLoop ? "gpu_resident_accumulation" : "cpu_host")
                                   : accumulation.residency;
  const std::string pathStatePath = pathStateResidency.empty()
                                      ? (fullGpuLoop ? "gpu_resident_path_state" : "cpu_host")
                                      : pathStateResidency;
  const std::string compactionPath =
    frontierCompactionExecutionPath.empty()
      ? (fullGpuLoop ? pathLoopExecution : "cpu_diffuse_frontier_compaction")
      : frontierCompactionExecutionPath;
  const bool compactionUsesGpu = executionPathUsesGpu(compactionPath);

  TracingExecutionCapabilityRecords records;
  if (fullGpuLoop) {
    records.intersection.closestHit =
      gpuRecord(Domain::Intersection, "geometry.closest_hit", closestHitPath);
    records.intersection.anyHit = gpuRecord(Domain::Intersection, "geometry.any_hit", anyHitPath);

    records.scene.geometryRecords =
      gpuRecord(Domain::SceneRecords, "scene.geometry_records", "gpu_tracing_scene_records");
    records.scene.materialRecords =
      gpuRecord(Domain::SceneRecords, "scene.material_records", "gpu_tracing_scene_records");
    records.scene.textureRecords =
      gpuRecord(Domain::SceneRecords, "scene.texture_records", "gpu_tracing_scene_records");
    records.scene.lightRecords =
      gpuRecord(Domain::SceneRecords, "scene.light_records", "gpu_tracing_scene_records");

    records.sampling.gpuRng = gpuRecord(Domain::Sampling, "sampling.gpu_rng", "gpu_sample_stream");
    records.sampling.namedDimensions =
      gpuRecord(Domain::Sampling, "sampling.named_dimensions", "gpu_sample_stream");

    records.directLighting.lightSampling =
      gpuRecord(Domain::DirectLighting, "lighting.direct_light_sample", pathLoopExecution);
    records.directLighting.visibility =
      gpuRecord(Domain::DirectLighting, "lighting.direct_light_visibility", anyHitPath);
    records.directLighting.contribution = gpuRecord(
      Domain::DirectLighting, "lighting.direct_light_contribution", directLightContributionPath);
    records.directLighting.residentBatch = gpuRecord(
      Domain::DirectLighting, "lighting.resident_direct_light_batches", pathLoopExecution);

    records.bsdf.eval = gpuRecord(Domain::BSDF, "shading.bsdf_eval", pathLoopExecution);
    records.bsdf.sample = gpuRecord(Domain::BSDF, "shading.bsdf_sample", pathLoopExecution);
    records.bsdf.deltaBranches = TracingCapabilityRecord::unsupported(
      Domain::BSDF, "shading.delta_branches",
      "compiled diffuse path loop supports diffuse continuation only");

    records.pathState.residency =
      gpuRecord(Domain::PathState, "state.path_state_residency", pathStatePath);
    records.pathState.frontierCompaction =
      gpuRecord(Domain::PathState, "state.frontier_compaction", compactionPath);
    records.pathState.spawnedContinuations =
      gpuRecord(Domain::PathState, "state.spawned_continuations", pathLoopExecution);

    records.accumulation.sampleAccumulation =
      gpuRecord(Domain::Accumulation, "accumulation.sample_accumulation", accumulationPath);
    records.accumulation.progressiveReadback = TracingCapabilityRecord::hybrid(
      Domain::Accumulation, "accumulation.progressive_readback", accumulationResidency);
    return records;
  }

  records.intersection.closestHit =
    fallback(Domain::Intersection, "geometry.closest_hit", closestHitPath);
  records.intersection.anyHit = fallback(Domain::Intersection, "geometry.any_hit", anyHitPath);

  records.scene.geometryRecords =
    restrictedCpuRecord(Domain::SceneRecords, "scene.geometry_records");
  records.scene.materialRecords =
    restrictedCpuRecord(Domain::SceneRecords, "scene.material_records");
  records.scene.textureRecords = restrictedCpuRecord(Domain::SceneRecords, "scene.texture_records");
  records.scene.lightRecords = restrictedCpuRecord(Domain::SceneRecords, "scene.light_records");

  records.sampling.gpuRng = TracingCapabilityRecord::restricted(
    Domain::Sampling, "sampling.gpu_rng", Device::CPU, "gpu_sample_stream_cpu_reference",
    "GPU sample stream dimensions are generated by the CPU reference path loop");
  records.sampling.namedDimensions = TracingCapabilityRecord::cpu(
    Domain::Sampling, "sampling.named_dimensions", "gpu_sample_stream");

  records.directLighting.lightSampling = fallbackWithReason(
    Domain::DirectLighting, "lighting.direct_light_sample", kCpuRecordExecutionPath,
    compiledDiffusePathLoopDirectLightSamplingFallbackReason());
  records.directLighting.visibility =
    fallbackWithReason(Domain::DirectLighting, "lighting.direct_light_visibility", anyHitPath,
                       compiledDiffusePathLoopDirectLightVisibilityFallbackReason());
  records.directLighting.contribution = fallbackWithReason(
    Domain::DirectLighting, "lighting.direct_light_contribution", directLightContributionPath,
    compiledDiffusePathLoopDirectLightContributionFallbackReason());
  records.directLighting.residentBatch = TracingCapabilityRecord::unsupported(
    Domain::DirectLighting, "lighting.resident_direct_light_batches",
    compiledDiffusePathLoopResidentDirectLightUnavailableReason());

  records.bsdf.eval = fallbackWithReason(Domain::BSDF, "shading.bsdf_eval", kCpuRecordExecutionPath,
                                         compiledDiffusePathLoopBsdfEvalFallbackReason());
  records.bsdf.sample =
    fallbackWithReason(Domain::BSDF, "shading.bsdf_sample", kCpuRecordExecutionPath,
                       compiledDiffusePathLoopBsdfSampleFallbackReason());
  records.bsdf.deltaBranches = TracingCapabilityRecord::unsupported(
    Domain::BSDF, "shading.delta_branches",
    "compiled diffuse path loop supports diffuse continuation only");

  records.pathState.residency =
    fallbackWithReason(Domain::PathState, "state.path_state_residency", pathStatePath,
                       compiledDiffusePathLoopPathStateResidencyFallbackReason());
  if (compactionUsesGpu) {
    records.pathState.frontierCompaction =
      gpuRecord(Domain::PathState, "state.frontier_compaction", compactionPath);
  } else {
    records.pathState.frontierCompaction =
      fallbackWithReason(Domain::PathState, "state.frontier_compaction", compactionPath,
                         compiledDiffusePathLoopFrontierCompactionFallbackReason());
  }
  records.pathState.spawnedContinuations =
    fallbackWithReason(Domain::PathState, "state.spawned_continuations", kCpuRecordExecutionPath,
                       compiledDiffusePathLoopSpawnedContinuationsFallbackReason());

  records.accumulation.sampleAccumulation =
    fallbackWithReason(Domain::Accumulation, "accumulation.sample_accumulation", accumulationPath,
                       compiledDiffusePathLoopSampleAccumulationFallbackReason());
  records.accumulation.progressiveReadback = TracingCapabilityRecord::cpu(
    Domain::Accumulation, "accumulation.progressive_readback", accumulationResidency);
  return records;
}

double GpuDiffusePathLoopResult::removedPathFraction() const {
  return fraction(removedPathCount(), inputPathCount());
}

double GpuDiffusePathLoopResult::movedRetainedPathFraction() const {
  return fraction(movedPathCount(), retainedPathCount());
}

GpuDiffusePathStepResult
GpuDiffusePathStepReference::step(const GpuTracingSceneSections& scene,
                                  const std::vector<GpuDiffusePathStateRecord>& pathStates,
                                  const std::vector<GpuIntersectionHitRecord>& closestHits,
                                  const GpuDiffusePathLoopSettings& settings) const {
  GpuDiffusePathStepResult result;
  result.closestHitRecords = closestHits;
  result.stepRecords.resize(pathStates.size());
  result.pathStates.reserve(pathStates.size());
  result.terminatedPathStates.reserve(pathStates.size());

  const std::map<std::uint32_t, GpuIntersectionHitRecord> hitRecords =
    hitsByRayIndex(closestHits, pathStates);
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
      const Colord contribution = Colord(pathState.throughput) * missRadiance(scene, pathState);
      pathState.accumulatedRadiance =
        (Colord(pathState.accumulatedRadiance) + contribution).toFloat4(0.0f);
      stepRecord.event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss);
      stepRecord.missRadiance = contribution.toFloat4(0.0f);
      terminate(pathState);
      stepRecord.flags = pathState.flags;
      result.terminatedPathStates.push_back(pathState);
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
      result.terminatedPathStates.push_back(pathState);
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
        (normal * wi) > 0.0 ? textureColor(scene, material.emissionTexture, hit) : Colord::black();
      const Colord contribution = throughput * emitted;
      accumulated += contribution;
      pathState.accumulatedRadiance = accumulated.toFloat4(0.0f);
      stepRecord.emittedRadiance = contribution.toFloat4(0.0f);
      terminate(pathState);
      stepRecord.flags = pathState.flags;
      result.terminatedPathStates.push_back(pathState);
      ++result.metrics.emissiveHits;
      ++result.metrics.terminatedPaths;
      continue;
    }

    if (!isDiffusePathLoopMaterial(materialKind)) {
      markUnsupported(pathState);
      stepRecord.event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Unsupported);
      stepRecord.flags = pathState.flags;
      result.terminatedPathStates.push_back(pathState);
      ++result.metrics.unsupportedHits;
      ++result.metrics.terminatedPaths;
      continue;
    }

    const Colord bsdf = diffuseBsdf(scene, material, hit);
    if (!scene.lights.empty()) {
      Colord directLightRadiance = Colord::black();
      const std::uint32_t configuredDirectLightSamples = directLightSampleCount(settings);
      for (std::uint32_t sampleIndex = 0; sampleIndex != configuredDirectLightSamples;
           ++sampleIndex) {
        const double selectionSample =
          sample1D(pathState, directLightSelectionDimension(pathState, sampleIndex));
        std::uint32_t lightIndex =
          std::min(static_cast<std::uint32_t>(selectionSample * scene.lights.size()),
                   static_cast<std::uint32_t>(scene.lights.size() - 1));
        const double selectionPdf = 1.0 / static_cast<double>(scene.lights.size());
        const LightSampleRecord light = sampleLight(
          scene.lights[lightIndex], point,
          sample2D(pathState, directLightSurfaceDimension(pathState, lightIndex, sampleIndex)));
        if (light.valid) {
          result.metrics.directLightVisibilityExecutionPath = kPackedCpuExecutionPath;
          result.metrics.directLightContributionExecutionPath = kCpuRecordExecutionPath;
          ++result.metrics.directLightSamples;
          const Rayd shadowRay = Rayd(Vector4d(hit.point), light.direction).epsilonShifted();
          const std::uint32_t shadowRayIndex =
            static_cast<std::uint32_t>(result.directLightShadowRays.size());
          const GpuIntersectionRay packedShadowRay =
            packRay(shadowRay, shadowRayIndex, kPackedRayMinimumDistance, light.distance);
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
            directLightRadiance += contribution;
          }
        }
      }

      directLightRadiance = directLightRadiance / static_cast<double>(configuredDirectLightSamples);
      accumulated += directLightRadiance;
      stepRecord.directLightRadiance = directLightRadiance.toFloat4(0.0f);
    }

    const Vector2d bsdfSample =
      sample2D(pathState, sampleDimension(pathState, kBsdfSampleDimensionOffset));
    const Vector3d wo = cosineHemisphereDirection(normal, bsdfSample);
    const double pdf = cosineHemispherePdf(normal, wo);
    const double normalDotOut = normal * wo;
    Colord nextThroughput =
      pdf <= 0.0 ? Colord::black() : throughput * (bsdf * (normalDotOut / pdf));
    if (pathState.depth >= settings.russianRouletteDepth) {
      const double roulette =
        sample1D(pathState, sampleDimension(pathState, kContinuationDimensionOffset));
      const PathContinuation continuation = pathContinuation(nextThroughput, roulette);
      nextThroughput = continuedThroughput(nextThroughput, continuation);
    }

    if (nextThroughput == Colord::black()) {
      pathState.accumulatedRadiance = accumulated.toFloat4(0.0f);
      terminate(pathState);
      stepRecord.flags = pathState.flags;
      result.terminatedPathStates.push_back(pathState);
      ++result.metrics.terminatedPaths;
      continue;
    }

    pathState.ray = packRay(Rayd(Vector4d(hit.point), wo).epsilonShifted(), pathState.ray.rayIndex,
                            kPackedRayMinimumDistance, std::numeric_limits<double>::infinity());
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

GpuDiffusePathLoopResult
GpuDiffusePathLoop::run(const GpuTracingSceneSections& scene,
                        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                        const GpuDiffusePathLoopSettings& settings) const {
  return run(scene, initialPathStates, settings,
             CpuReferenceGpuDiffusePathFrontierCompactionBackend::instance());
}

GpuDiffusePathLoopResult
GpuDiffusePathLoop::run(const GpuTracingSceneSections& scene,
                        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                        const GpuDiffusePathLoopSettings& settings,
                        const GpuDiffusePathFrontierCompactionBackend& compactionBackend) const {
  GpuDiffusePathLoopResult result;
  result.initialPathCount = initialPathStates.size();
  result.frontierCompactionExecutionPath = compactionBackend.name();
  result.frontierCompactionPathStateResidency = compactionBackend.pathStateResidency();

  std::vector<GpuDiffusePathStateRecord> active;
  active.reserve(initialPathStates.size());
  for (GpuDiffusePathStateRecord pathState : initialPathStates) {
    if (gpuDiffusePathStateIsActive(pathState) && pathState.depth < settings.maxDepth) {
      active.push_back(pathState);
      continue;
    }

    if (gpuDiffusePathStateIsActive(pathState)) {
      terminate(pathState);
      ++result.maxDepthTerminatedPaths;
      ++result.metrics.terminatedPaths;
    }
    if (gpuDiffusePathStateIsTerminated(pathState)) {
      result.resolvedPathStates.push_back(pathState);
    }
  }

  GpuDiffusePathStep stepper;
  while (!active.empty()) {
    result.activePathsPerDepth.push_back(active.size());
    const GpuDiffusePathStepResult step = stepper.step(scene, active, settings);
    ++result.depthCount;
    result.metrics.merge(step.metrics);
    result.stepRecords.insert(result.stepRecords.end(), step.stepRecords.begin(),
                              step.stepRecords.end());
    result.resolvedPathStates.insert(result.resolvedPathStates.end(),
                                     step.terminatedPathStates.begin(),
                                     step.terminatedPathStates.end());

    std::vector<std::uint32_t> retainedPathIndices;
    retainedPathIndices.reserve(step.pathStates.size());
    for (std::size_t index = 0; index != step.pathStates.size(); ++index) {
      GpuDiffusePathStateRecord pathState = step.pathStates[index];
      if (pathState.depth >= settings.maxDepth) {
        terminate(pathState);
        result.resolvedPathStates.push_back(pathState);
        ++result.maxDepthTerminatedPaths;
        ++result.metrics.terminatedPaths;
      } else {
        if (index > std::numeric_limits<std::uint32_t>::max()) {
          throw std::overflow_error(
            "gpu diffuse path frontier compaction path index exceeds GPU index range");
        }
        retainedPathIndices.push_back(static_cast<std::uint32_t>(index));
      }
    }

    const GpuDiffusePathFrontierCompactionResult compaction =
      compactionBackend.compact(step.pathStates, retainedPathIndices);
    if (compaction.inputPathCount != step.pathStates.size()) {
      throw std::logic_error(
        "gpu diffuse path frontier compaction backend returned a mismatched input path count");
    }
    if (compaction.retainedRecords.size() != compaction.retainedPathCount()) {
      throw std::logic_error(
        "gpu diffuse path frontier compaction backend returned a mismatched retained path count");
    }
    result.retainedIndexBytes =
      saturatedAdd(result.retainedIndexBytes, compaction.retainedIndexBytes());
    result.frontierCompactionUploadWorkerSeconds += compaction.uploadWorkerSeconds;
    result.frontierCompactionKernelWorkerSeconds += compaction.kernelWorkerSeconds;
    result.frontierCompactionReadbackWorkerSeconds += compaction.readbackWorkerSeconds;
    result.frontierCompactionExecutionPath = compaction.executionPath;
    result.frontierCompactionPathStateResidency = compaction.pathStateResidency;

    active = compaction.retainedRecords;
  }

  return result;
}

TracingAccumulationDiagnostics
render::resolveGpuDiffusePathLoopImage(const std::vector<GpuDiffusePathStateRecord>& records,
                                       const TracingAccumulationLayout& layout,
                                       Buffer<Colord>& target) {
  TracingAccumulationBuffer accumulation(layout);
  TracingAccumulationDiagnostics diagnostics =
    accumulateGpuDiffusePathLoopImage(records, layout, accumulation);
  if (target.width() != layout.width || target.height() != layout.height) {
    throw std::invalid_argument("gpu diffuse path-loop HDR target dimensions do not match layout");
  }
  for (int y = 0; y != layout.height; ++y) {
    for (int x = 0; x != layout.width; ++x) {
      target[y][x] = accumulation.resolvedColor(x, y);
    }
  }
  diagnostics.recordResolve();
  diagnostics.recordReadback(layout.colorSumBytes());
  return diagnostics;
}

TracingAccumulationDiagnostics
render::resolveGpuDiffusePathLoopImage(const GpuDiffusePathLoopResult& result,
                                       const TracingAccumulationLayout& layout,
                                       Buffer<Colord>& target) {
  if (platformAccumulationMatchesLayout(result, layout)) {
    return resolvePlatformAccumulationImage(result, layout, target);
  }
  return render::resolveGpuDiffusePathLoopImage(result.resolvedPathStates, layout, target);
}

TracingAccumulationDiagnostics
render::resolveGpuDiffusePathLoopImage(const std::vector<GpuDiffusePathStateRecord>& records,
                                       const TracingAccumulationLayout& layout,
                                       Buffer<unsigned int>& target, const Tonemap* tonemap) {
  TracingAccumulationBuffer accumulation(layout);
  TracingAccumulationDiagnostics diagnostics =
    accumulateGpuDiffusePathLoopImage(records, layout, accumulation);
  accumulation.resolve(target, tonemap);
  diagnostics.recordResolve();
  diagnostics.recordReadback(layout.resolveBytes());
  return diagnostics;
}

TracingAccumulationDiagnostics
render::resolveGpuDiffusePathLoopImage(const GpuDiffusePathLoopResult& result,
                                       const TracingAccumulationLayout& layout,
                                       Buffer<unsigned int>& target, const Tonemap* tonemap) {
  if (platformAccumulationMatchesLayout(result, layout)) {
    return resolvePlatformAccumulationImage(result, layout, target, tonemap);
  }
  return render::resolveGpuDiffusePathLoopImage(result.resolvedPathStates, layout, target, tonemap);
}
