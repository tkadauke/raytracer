#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Constants.h"
#include "core/math/Matrix.h"
#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

#include "render/GpuDiffusePathLoopBackend.h"
#include "render/GpuDiffusePathLoopLaunch.h"
#include "render/GpuDiffusePathStepReference.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/MetalGpuDiffusePathLoopBackend.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalGpuDiffusePathFrontierCompactionBackend.h"
#include "render/MetalGpuDiffusePathLoopKernel.h"
#endif
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanGpuDiffusePathFrontierCompactionBackend.h"
#endif
#include "render/VulkanGpuDiffusePathLoopBackend.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/PointLight.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Disk.h"
#include "render/primitives/Instance.h"
#include "render/primitives/OpenCylinder.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Triangle.h"
#include "render/samplers/GpuSampleStream.h"
#include "render/samplers/Sampler.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"
#include "render/textures/mappings/UVMapping2D.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace GpuDiffusePathStepReferenceTest {
  using namespace render;

  namespace {
    class UnsupportedGpuTracingMaterial final : public Material {
    public:
      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }
    };

    Colord colorFrom4(const std::array<float, 4>& value) {
      return Colord(value);
    }

    Vector3d vectorFrom4(const std::array<float, 4>& value) {
      return Vector3d(value);
    }

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    std::vector<std::uint32_t> sortedRetainedPathIndices(std::vector<std::uint32_t> indices) {
      std::sort(indices.begin(), indices.end());
      return indices;
    }
#endif

    GpuDiffusePathStateRecord activePath(std::uint32_t rayIndex = 7) {
      GpuDiffusePathStateRecord path = makeActiveGpuDiffusePathState();
      path.ray = GpuIntersectionScenePacker().packRay(
        Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), rayIndex);
      path.pixelIndex = 3;
      path.primarySampleIndex = 2;
      path.sampleSeed = 12345;
      path.sampleDimensionBase = static_cast<std::uint32_t>(SampleDimension::BSDF);
      path.sampleDimensionStride = 4;
      return path;
    }

    GpuDiffusePathStateRecord activePath(const Rayd& ray, std::uint32_t rayIndex) {
      GpuDiffusePathStateRecord path = activePath(rayIndex);
      path.ray = GpuIntersectionScenePacker().packRay(ray, rayIndex);
      return path;
    }

    class RecordingFrontierCompactionBackend final
        : public GpuDiffusePathFrontierCompactionBackend {
    public:
      const char* name() const override {
        return "recording_diffuse_frontier_compaction";
      }

      const char* pathStateResidency() const override {
        return "recording_path_state";
      }

      GpuDiffusePathFrontierCompactionResult
      compact(const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
              const std::vector<std::uint32_t>& retainedPathIndices) const override {
        ++calls;
        inputCounts.push_back(sourceRecords.size());
        retainedIndices.push_back(retainedPathIndices);

        GpuDiffusePathFrontierCompactionResult result;
        result.executionPath = name();
        result.pathStateResidency = pathStateResidency();
        result.inputPathCount = sourceRecords.size();
        result.retainedPathIndices = retainedPathIndices;
        result.uploadWorkerSeconds = uploadWorkerSeconds;
        result.kernelWorkerSeconds = kernelWorkerSeconds;
        result.readbackWorkerSeconds = readbackWorkerSeconds;
        for (const std::uint32_t index : retainedPathIndices) {
          result.retainedRecords.push_back(sourceRecords[index]);
        }
        return result;
      }

      mutable int calls{0};
      mutable std::vector<std::size_t> inputCounts;
      mutable std::vector<std::vector<std::uint32_t>> retainedIndices;
      double uploadWorkerSeconds{0.01};
      double kernelWorkerSeconds{0.02};
      double readbackWorkerSeconds{0.03};
    };

    class AvailableFullGpuPathLoopBackend : public GpuDiffusePathLoopBackend {
    public:
      const char* name() const override {
        return "available_full_gpu_path_loop";
      }

      bool fullGpuPathLoopAvailable() const override {
        return true;
      }

      GpuDiffusePathLoopResult run(const GpuTracingSceneSections&,
                                   const std::vector<GpuDiffusePathStateRecord>&,
                                   const GpuDiffusePathLoopSettings&) const override {
        return {};
      }
    };

    class SceneRejectingFullGpuPathLoopBackend final : public AvailableFullGpuPathLoopBackend {
    public:
      GpuDiffusePathLoopBackendSupport
      fullGpuPathLoopSupport(const GpuTracingSceneSections&) const override {
        return {false, "test backend supports only a narrower scene subset"};
      }
    };

    GpuIntersectionHitRecord hitRecord(std::uint32_t rayIndex, std::uint32_t material) {
      GpuIntersectionHitRecord hit;
      hit.hit = 1;
      hit.rayIndex = rayIndex;
      hit.material = material;
      hit.object = 9;
      hit.primitiveRecord = 5;
      hit.distance = 4.0f;
      hit.point = {0.0f, 0.0f, 0.0f, 1.0f};
      hit.normal = {0.0f, 0.0f, -1.0f, 0.0f};
      return hit;
    }

    GpuTracingSceneSections sectionsFor(Scene& scene) {
      const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
      GpuTracingSceneSections sections;
      sections.geometry = GpuIntersectionScenePacker().packScene(intersection);
      const GpuTracingMaterialCompilation materials = compileGpuTracingMaterials(intersection);
      sections.materials = materials.records;
      sections.textures = materials.textures.records;
      sections.lights = compileGpuTracingLights(scene).records;
      sections.environment.push_back(makeGpuTracingConstantEnvironment(scene.background()));
      if (scene.environmentRadiance() != scene.background()) {
        sections.environment.push_back(
          makeGpuTracingConstantEnvironment(scene.environmentRadiance()));
      }
      return sections;
    }

    std::uint32_t firstMaterialId(const GpuTracingSceneSections& sections,
                                  GpuTracingMaterialKind kind) {
      for (std::uint32_t index = 0; index != sections.materials.size(); ++index) {
        if (sections.materials[index].kind == static_cast<std::uint32_t>(kind)) {
          return index;
        }
      }
      return 0;
    }

    void expectFloat4Near(const std::array<float, 4>& actual, const std::array<float, 4>& expected,
                          double tolerance = 1e-5) {
      for (std::size_t index = 0; index != actual.size(); ++index) {
        EXPECT_NEAR(expected[index], actual[index], tolerance);
      }
    }

    void expectHitRecordNear(const GpuIntersectionHitRecord& actual,
                             const GpuIntersectionHitRecord& expected, double tolerance = 1e-5) {
      EXPECT_EQ(expected.hit, actual.hit);
      EXPECT_EQ(expected.material, actual.material);
      EXPECT_EQ(expected.object, actual.object);
      EXPECT_EQ(expected.primitiveRecord, actual.primitiveRecord);
      EXPECT_EQ(expected.rayIndex, actual.rayIndex);
      if (std::isinf(expected.distance) || std::isinf(actual.distance)) {
        EXPECT_EQ(expected.distance, actual.distance);
      } else {
        EXPECT_NEAR(expected.distance, actual.distance, tolerance);
      }
      expectFloat4Near(actual.point, expected.point, tolerance);
      expectFloat4Near(actual.normal, expected.normal, tolerance);
      expectFloat4Near(actual.uv, expected.uv, tolerance);
      expectFloat4Near(actual.barycentric, expected.barycentric, tolerance);
    }

    void expectStepRecordEqual(const GpuDiffusePathStepRecord& actual,
                               const GpuDiffusePathStepRecord& expected) {
      EXPECT_EQ(expected.event, actual.event);
      EXPECT_EQ(expected.pathIndex, actual.pathIndex);
      EXPECT_EQ(expected.pixelIndex, actual.pixelIndex);
      EXPECT_EQ(expected.primarySampleIndex, actual.primarySampleIndex);
      EXPECT_EQ(expected.depth, actual.depth);
      EXPECT_EQ(expected.material, actual.material);
      EXPECT_EQ(expected.object, actual.object);
      EXPECT_EQ(expected.flags, actual.flags);
      EXPECT_EQ(expected.emittedRadiance, actual.emittedRadiance);
      EXPECT_EQ(expected.directLightRadiance, actual.directLightRadiance);
      EXPECT_EQ(expected.missRadiance, actual.missRadiance);
      EXPECT_EQ(expected.continuationThroughput, actual.continuationThroughput);
    }

    void expectGpuRayNear(const GpuIntersectionRay& actual, const GpuIntersectionRay& expected,
                          double tolerance = 1e-5) {
      expectFloat4Near(actual.origin, expected.origin, tolerance);
      expectFloat4Near(actual.direction, expected.direction, tolerance);
      EXPECT_NEAR(expected.minDistance, actual.minDistance, tolerance);
      if (std::isinf(expected.maxDistance) || std::isinf(actual.maxDistance)) {
        EXPECT_EQ(expected.maxDistance, actual.maxDistance);
      } else {
        EXPECT_NEAR(expected.maxDistance, actual.maxDistance, tolerance);
      }
      EXPECT_NEAR(expected.timeSample, actual.timeSample, tolerance);
      EXPECT_EQ(expected.flags, actual.flags);
      EXPECT_EQ(expected.rayIndex, actual.rayIndex);
    }

    void expectPathStateNear(const GpuDiffusePathStateRecord& actual,
                             const GpuDiffusePathStateRecord& expected, double tolerance = 1e-5) {
      expectGpuRayNear(actual.ray, expected.ray, tolerance);
      expectFloat4Near(actual.throughput, expected.throughput, tolerance);
      expectFloat4Near(actual.accumulatedRadiance, expected.accumulatedRadiance, tolerance);
      EXPECT_EQ(expected.pixelIndex, actual.pixelIndex);
      EXPECT_EQ(expected.primarySampleIndex, actual.primarySampleIndex);
      EXPECT_EQ(expected.depth, actual.depth);
      EXPECT_EQ(expected.sampleSeed, actual.sampleSeed);
      EXPECT_EQ(expected.sampleDimensionBase, actual.sampleDimensionBase);
      EXPECT_EQ(expected.sampleDimensionStride, actual.sampleDimensionStride);
      EXPECT_EQ(expected.flags, actual.flags);
      EXPECT_NEAR(expected.previousBsdfPdf, actual.previousBsdfPdf, tolerance);
      EXPECT_NEAR(expected.previousLightPdf, actual.previousLightPdf, tolerance);
      EXPECT_EQ(expected.previousMaterial, actual.previousMaterial);
      EXPECT_EQ(expected.previousEventFlags, actual.previousEventFlags);
    }

    void expectOcclusionRecordEqual(const GpuIntersectionOcclusionRecord& actual,
                                    const GpuIntersectionOcclusionRecord& expected) {
      EXPECT_EQ(expected.occluded, actual.occluded);
      EXPECT_EQ(expected.rayIndex, actual.rayIndex);
    }

    void expectStepResultParity(const GpuDiffusePathStepResult& actual,
                                const GpuDiffusePathStepResult& expected) {
      ASSERT_EQ(expected.closestHitRecords.size(), actual.closestHitRecords.size());
      for (std::size_t index = 0; index != actual.closestHitRecords.size(); ++index) {
        expectHitRecordNear(actual.closestHitRecords[index], expected.closestHitRecords[index]);
      }

      ASSERT_EQ(expected.stepRecords.size(), actual.stepRecords.size());
      for (std::size_t index = 0; index != actual.stepRecords.size(); ++index) {
        expectStepRecordEqual(actual.stepRecords[index], expected.stepRecords[index]);
      }

      ASSERT_EQ(expected.pathStates.size(), actual.pathStates.size());
      for (std::size_t index = 0; index != actual.pathStates.size(); ++index) {
        expectPathStateNear(actual.pathStates[index], expected.pathStates[index]);
      }

      ASSERT_EQ(expected.terminatedPathStates.size(), actual.terminatedPathStates.size());
      for (std::size_t index = 0; index != actual.terminatedPathStates.size(); ++index) {
        expectPathStateNear(actual.terminatedPathStates[index],
                            expected.terminatedPathStates[index]);
      }

      ASSERT_EQ(expected.directLightShadowRays.size(), actual.directLightShadowRays.size());
      for (std::size_t index = 0; index != actual.directLightShadowRays.size(); ++index) {
        expectGpuRayNear(actual.directLightShadowRays[index],
                         expected.directLightShadowRays[index]);
      }

      ASSERT_EQ(expected.directLightOcclusionRecords.size(),
                actual.directLightOcclusionRecords.size());
      for (std::size_t index = 0; index != actual.directLightOcclusionRecords.size(); ++index) {
        expectOcclusionRecordEqual(actual.directLightOcclusionRecords[index],
                                   expected.directLightOcclusionRecords[index]);
      }
    }

    std::vector<GpuIntersectionHitRecord>
    closestHitsFor(const GpuTracingSceneSections& sections,
                   const std::vector<GpuDiffusePathStateRecord>& paths) {
      std::vector<GpuIntersectionRay> rays;
      for (const GpuDiffusePathStateRecord& path : paths) {
        if (gpuDiffusePathStateIsActive(path)) {
          rays.push_back(path.ray);
        }
      }
      return GpuIntersectionIntersector().intersectClosest(sections.geometry, rays);
    }

    Vector3d expectedCosineHemisphereDirection(const Vector3d& normal, const Vector2d& sample) {
      const double r = std::sqrt(sample.x());
      const double phi = TAU * sample.y();
      const Vector3d helper = std::abs(normal.y()) < 0.999 ? Vector3d::up() : Vector3d::right();
      const Vector3d tangent = (helper ^ normal).normalized();
      const Vector3d bitangent = normal ^ tangent;
      return (tangent * (r * std::cos(phi)) + bitangent * (r * std::sin(phi)) +
              normal * std::sqrt(std::max(0.0, 1.0 - sample.x())))
        .normalized();
    }
  }

  TEST(GpuDiffusePathStep, RunsClosestHitAndMaterialLookupForActivePaths) {
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto matteSphere = std::make_shared<Sphere>(Vector3d(-1.0, 0.0, 0.0), 0.5);
    matteSphere->setMaterial(matte);

    auto emissive = std::make_shared<EmissiveMaterial>(Colord(0.25, 0.5, 0.75));
    auto emissiveSphere = std::make_shared<Sphere>(Vector3d(1.0, 0.0, 0.0), 0.5);
    emissiveSphere->setMaterial(emissive);

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    scene.add(matteSphere);
    scene.add(emissiveSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);

    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(-1.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 11),
      activePath(Rayd(Vector4d(1.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 12),
      activePath(Rayd(Vector4d(3.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 13),
      makeTerminatedGpuDiffusePathState(),
    };
    paths[3].ray.rayIndex = 99;

    const std::vector<GpuIntersectionHitRecord> cpuClosestHits =
      GpuIntersectionIntersector().intersectClosest(sections.geometry,
                                                    {paths[0].ray, paths[1].ray, paths[2].ray});
    const GpuDiffusePathStepResult cpuReference =
      GpuDiffusePathStepReference().step(sections, paths, cpuClosestHits);

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, paths);

    ASSERT_EQ(cpuClosestHits.size(), result.closestHitRecords.size());
    for (std::size_t index = 0; index != cpuClosestHits.size(); ++index) {
      expectHitRecordNear(result.closestHitRecords[index], cpuClosestHits[index]);
    }
    ASSERT_EQ(cpuReference.stepRecords.size(), result.stepRecords.size());
    for (std::size_t index = 0; index != cpuReference.stepRecords.size(); ++index) {
      expectStepRecordEqual(result.stepRecords[index], cpuReference.stepRecords[index]);
    }

    ASSERT_TRUE(result.closestHitRecords[0].hit);
    ASSERT_TRUE(result.closestHitRecords[1].hit);
    EXPECT_FALSE(result.closestHitRecords[2].hit);
    EXPECT_EQ(firstMaterialId(sections, GpuTracingMaterialKind::Matte),
              result.closestHitRecords[0].material);
    EXPECT_EQ(firstMaterialId(sections, GpuTracingMaterialKind::Emissive),
              result.closestHitRecords[1].material);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[1].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[2].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive),
              result.stepRecords[3].event);
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);
    EXPECT_EQ(3u, result.metrics.closestHitRays);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, GeneratesOneActivePathPerPixelSample) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    EXPECT_EQ(0, generation.requestedRect.left());
    EXPECT_EQ(0, generation.requestedRect.top());
    EXPECT_EQ(3, generation.requestedRect.width());
    EXPECT_EQ(2, generation.requestedRect.height());
    EXPECT_EQ(0, generation.actualRect.left());
    EXPECT_EQ(0, generation.actualRect.top());
    EXPECT_EQ(3, generation.actualRect.width());
    EXPECT_EQ(2, generation.actualRect.height());
    EXPECT_EQ(24u, generation.generatedPrimarySamples);
    EXPECT_EQ(0u, generation.skippedPrimarySamples);
    ASSERT_EQ(24u, generation.pathStates.size());

    std::size_t pathIndex = 0;
    for (std::uint32_t pixelIndex = 0; pixelIndex != 6; ++pixelIndex) {
      for (std::uint32_t sampleIndex = 0; sampleIndex != 4; ++sampleIndex) {
        const GpuDiffusePathStateRecord& path = generation.pathStates[pathIndex];
        EXPECT_TRUE(gpuDiffusePathStateIsActive(path));
        EXPECT_FALSE(gpuDiffusePathStateIsTerminated(path));
        EXPECT_EQ(static_cast<std::uint32_t>(pathIndex), path.ray.rayIndex);
        EXPECT_EQ(pixelIndex, path.pixelIndex);
        EXPECT_EQ(sampleIndex, path.primarySampleIndex);
        EXPECT_EQ(0u, path.depth);
        EXPECT_EQ(1234u, path.sampleSeed);
        EXPECT_EQ(static_cast<std::uint32_t>(SampleDimension::BSDF), path.sampleDimensionBase);
        EXPECT_EQ(static_cast<std::uint32_t>(kPathSampleDimensionStride),
                  path.sampleDimensionStride);
        EXPECT_FLOAT_EQ(1.0f, path.throughput[0]);
        EXPECT_FLOAT_EQ(1.0f, path.throughput[1]);
        EXPECT_FLOAT_EQ(1.0f, path.throughput[2]);
        EXPECT_FLOAT_EQ(0.0f, path.accumulatedRadiance[0]);
        EXPECT_FLOAT_EQ(0.0f, path.accumulatedRadiance[1]);
        EXPECT_FLOAT_EQ(0.0f, path.accumulatedRadiance[2]);
        EXPECT_NEAR(1.0, Vector3d(path.ray.direction).length(), 1e-5);
        EXPECT_GE(path.ray.timeSample, 0.0f);
        EXPECT_LT(path.ray.timeSample, 1.0f);
        ++pathIndex;
      }
    }
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, UsesActualRenderableRectForFitExactCameras) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setAspectMode(AspectMode::FitExact);
    camera.setAspectRatio(1.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 7);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2));

    EXPECT_EQ(0, generation.requestedRect.left());
    EXPECT_EQ(0, generation.requestedRect.top());
    EXPECT_EQ(4, generation.requestedRect.width());
    EXPECT_EQ(2, generation.requestedRect.height());
    EXPECT_EQ(1, generation.actualRect.left());
    EXPECT_EQ(0, generation.actualRect.top());
    EXPECT_EQ(2, generation.actualRect.width());
    EXPECT_EQ(2, generation.actualRect.height());
    ASSERT_EQ(4u, generation.pathStates.size());
    EXPECT_EQ(1u, generation.pathStates[0].pixelIndex);
    EXPECT_EQ(2u, generation.pathStates[1].pixelIndex);
    EXPECT_EQ(5u, generation.pathStates[2].pixelIndex);
    EXPECT_EQ(6u, generation.pathStates[3].pixelIndex);
  }

  TEST(GpuDiffusePathStep, OneBounceMissMatchesReferenceRecordsAndContribution) {
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.25, 0.5, 0.75));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              actual.stepRecords[0].event);
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), Colord(actual.stepRecords[0].missRadiance),
                      1e-6);
    EXPECT_TRUE(actual.pathStates.empty());
    ASSERT_EQ(1u, actual.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(actual.terminatedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      Colord(actual.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ("packed_cpu", actual.metrics.closestHitExecutionPath);
    EXPECT_EQ(1u, actual.metrics.closestHitRays);
    EXPECT_EQ(1u, actual.metrics.misses);
    EXPECT_EQ(1u, actual.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, OneBounceDiffuseContinuationMatchesReferenceRecords) {
    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.sampleSeed = 12347;
    path.throughput = {0.2f, 0.4f, 0.6f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              actual.stepRecords[0].event);
    ASSERT_EQ(1u, actual.pathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(actual.pathStates[0]));
    EXPECT_EQ(1u, actual.pathStates[0].depth);
    ASSERT_COLOR_NEAR(Colord(expected.pathStates[0].throughput),
                      Colord(actual.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(actual.pathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(1u, actual.metrics.spawnedContinuations);
  }

  TEST(GpuDiffusePathStep, OneBounceDirectLightMatchesReferenceRecordsAndContribution) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.pathStates.size());
    ASSERT_EQ(1u, actual.directLightShadowRays.size());
    ASSERT_EQ(1u, actual.directLightOcclusionRecords.size());
    EXPECT_EQ(0u, actual.directLightOcclusionRecords[0].occluded);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(actual.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(actual.stepRecords[0].directLightRadiance),
                      Colord(actual.pathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ("packed_cpu", actual.metrics.directLightVisibilityExecutionPath);
    EXPECT_EQ("cpu_record", actual.metrics.directLightContributionExecutionPath);
    EXPECT_EQ(1u, actual.metrics.directLightSamples);
    EXPECT_EQ(1u, actual.metrics.directLightVisibilityRays);
    EXPECT_EQ(1u, actual.metrics.directLightContributionEvaluations);
    EXPECT_EQ(1u, actual.metrics.directLightContributingSamples);
  }

  TEST(GpuDiffusePathStep, DirectLightSamplesAreAveragedAcrossConfiguredSamples) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    GpuDiffusePathLoopSettings settings;
    settings.directLightSamples = 2;

    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths, settings);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.pathStates.size());
    ASSERT_EQ(2u, actual.directLightShadowRays.size());
    ASSERT_EQ(2u, actual.directLightOcclusionRecords.size());
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      colorFrom4(actual.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(colorFrom4(actual.stepRecords[0].directLightRadiance),
                      colorFrom4(actual.pathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(2u, actual.metrics.directLightSamples);
    EXPECT_EQ(2u, actual.metrics.directLightVisibilityRays);
    EXPECT_EQ(2u, actual.metrics.directLightContributionEvaluations);
    EXPECT_EQ(2u, actual.metrics.directLightContributingSamples);
  }

  TEST(GpuDiffusePathStep, UnsupportedMaterialLookupTerminatesExplicitly) {
    auto unsupportedSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    unsupportedSphere->setMaterial(std::make_shared<UnsupportedGpuTracingMaterial>());

    Scene scene;
    scene.add(unsupportedSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {activePath()});

    ASSERT_EQ(1u, result.closestHitRecords.size());
    EXPECT_TRUE(result.closestHitRecords[0].hit);
    ASSERT_LT(result.closestHitRecords[0].material, sections.materials.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported),
              sections.materials[result.closestHitRecords[0].material].kind);

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    EXPECT_NE(0u, result.terminatedPathStates[0].flags & gpuDiffusePathStateUnsupportedFlag);
    EXPECT_NE(0u, result.stepRecords[0].flags & gpuDiffusePathStateUnsupportedFlag);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Unsupported),
              result.stepRecords[0].event);
    EXPECT_EQ(result.closestHitRecords[0].material, result.stepRecords[0].material);
    EXPECT_EQ(1u, result.metrics.unsupportedHits);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, ReflectiveMaterialSpawnsExactMirrorContinuation) {
    auto reflective =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    reflective->setDiffuseCoefficient(0.0);
    reflective->setReflectionColor(Colord(0.75, 0.5, 0.25));
    reflective->setReflectionCoefficient(0.5);
    auto mirrorSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    mirrorSphere->setMaterial(reflective);

    Scene scene;
    scene.add(mirrorSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 1.0f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              actual.stepRecords[0].event);
    ASSERT_EQ(1u, actual.pathStates.size());
    EXPECT_TRUE(actual.terminatedPathStates.empty());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(actual.pathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.25 * 0.75 * 0.5, 0.5 * 0.5 * 0.5, 1.0 * 0.25 * 0.5),
                      colorFrom4(actual.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(colorFrom4(actual.pathStates[0].throughput),
                      colorFrom4(actual.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -1.0), vectorFrom4(actual.pathStates[0].ray.direction),
                       1e-6);
    EXPECT_EQ(1u, actual.pathStates[0].depth);
    EXPECT_FLOAT_EQ(1.0f, actual.pathStates[0].previousBsdfPdf);
    EXPECT_FLOAT_EQ(0.0f, actual.pathStates[0].previousLightPdf);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              actual.pathStates[0].previousEventFlags);
    EXPECT_EQ(1u, actual.metrics.spawnedContinuations);
    EXPECT_EQ(0u, actual.metrics.unsupportedHits);
    EXPECT_EQ(0u, actual.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, TransparentMaterialSpawnsRefractedDeltaContinuation) {
    auto transparent = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord::white()));
    transparent->setDiffuseCoefficient(0.0);
    transparent->setSpecularCoefficient(0.0);
    transparent->setReflectionCoefficient(0.0);
    transparent->setTransmissionCoefficient(1.0);
    transparent->setRefractionIndex(1.5);
    auto glassSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    glassSphere->setMaterial(transparent);

    Scene scene;
    scene.add(glassSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 1.0f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              actual.stepRecords[0].event);
    ASSERT_EQ(1u, actual.pathStates.size());
    EXPECT_TRUE(actual.terminatedPathStates.empty());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(actual.pathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 1.0) * (1.0 / (1.5 * 1.5)),
                      colorFrom4(actual.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(colorFrom4(actual.pathStates[0].throughput),
                      colorFrom4(actual.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 1.0), vectorFrom4(actual.pathStates[0].ray.direction),
                       1e-6);
    EXPECT_EQ(1u, actual.pathStates[0].depth);
    EXPECT_FLOAT_EQ(1.0f, actual.pathStates[0].previousBsdfPdf);
    EXPECT_FLOAT_EQ(0.0f, actual.pathStates[0].previousLightPdf);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              actual.pathStates[0].previousEventFlags);
    EXPECT_EQ(1u, actual.metrics.spawnedContinuations);
    EXPECT_EQ(0u, actual.metrics.unsupportedHits);
    EXPECT_EQ(0u, actual.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, PhongHitUsesDiffuseAndGlossyLobesForCompiledPathLoop) {
    Scene scene;
    auto phong = std::make_shared<PhongMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord::white(), 16.0);
    phong->setDiffuseCoefficient(0.8);
    phong->setSpecularCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(phong);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {path});

    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_EQ(1u, result.directLightShadowRays.size());
    ASSERT_EQ(1u, result.directLightOcclusionRecords.size());
    EXPECT_EQ(0u, result.directLightOcclusionRecords[0].occluded);
    const Colord finiteBsdf = Colord(0.25, 0.5, 0.75) * 0.8 * invPI + Colord::white();
    ASSERT_COLOR_NEAR(Colord(0.8, 0.6, 0.4) * finiteBsdf * Colord(0.5, 0.25, 0.125),
                      colorFrom4(result.stepRecords[0].directLightRadiance), 1e-5);
    EXPECT_NE(Colord::black(), colorFrom4(result.stepRecords[0].continuationThroughput));
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
    EXPECT_EQ(0u, result.metrics.unsupportedHits);
  }

  TEST(GpuDiffusePathStep, EmissiveHitFeedsContributionIntoStepRecord) {
    Scene scene;
    auto lightCard = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    lightCard->setMaterial(std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0)));
    scene.add(lightCard);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 0.75f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {path});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0),
                      Colord(result.terminatedPathStates[0].accumulatedRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0), Colord(result.stepRecords[0].emittedRadiance),
                      1e-5);
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);
    EXPECT_EQ("cpu_record", result.metrics.emissionExecutionPath);
    EXPECT_EQ(1u, result.metrics.emissiveHits);
    EXPECT_EQ(1u, result.metrics.emissionContributionEvaluations);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, MatteHitFeedsDirectLightContributionIntoNextPathState) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {activePath()});

    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_EQ(1u, result.directLightShadowRays.size());
    ASSERT_EQ(1u, result.directLightOcclusionRecords.size());
    EXPECT_EQ(0u, result.directLightOcclusionRecords[0].occluded);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(result.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(result.pathStates[0].accumulatedRadiance), 1e-5);
    EXPECT_TRUE(gpuDiffusePathStateIsActive(result.pathStates[0]));
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);
    EXPECT_EQ("packed_cpu", result.metrics.directLightVisibilityExecutionPath);
    EXPECT_EQ("cpu_record", result.metrics.directLightContributionExecutionPath);
    EXPECT_EQ(1u, result.metrics.directLightSamples);
    EXPECT_EQ(1u, result.metrics.directLightVisibilityRays);
    EXPECT_EQ(1u, result.metrics.directLightContributionEvaluations);
    EXPECT_EQ(1u, result.metrics.directLightContributingSamples);
  }

  TEST(GpuDiffusePathStepReference, PrimaryMissAddsVisibleBackgroundAndTerminatesPath) {
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.75, 0.5, 0.25));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {path}, {GpuIntersectionScenePacker().packMiss(7)});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      colorFrom4(result.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), colorFrom4(result.stepRecords[0].missRadiance),
                      1e-6);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    EXPECT_EQ(1u, result.metrics.misses);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStepReference, BouncedMissAddsEnvironmentRadianceAndTerminatesPath) {
    Scene scene;
    scene.setBackground(Colord(0.75, 0.5, 0.25));
    scene.setEnvironmentRadiance(Colord(0.25, 0.5, 0.75));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.depth = 1;
    path.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {path}, {GpuIntersectionScenePacker().packMiss(7)});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      Colord(result.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), Colord(result.stepRecords[0].missRadiance),
                      1e-6);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    EXPECT_EQ(1u, result.metrics.misses);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStepReference, RejectsClosestHitRecordCountMismatch) {
    GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(7), activePath(8)};
    const auto step = [&] {
      return GpuDiffusePathStepReference().step(sections, paths,
                                                {GpuIntersectionScenePacker().packMiss(7)});
    };

    EXPECT_THROW(
      {
        const GpuDiffusePathStepResult result = step();
        (void)result;
      },
      std::logic_error);
  }

  TEST(GpuDiffusePathStepReference, RejectsDuplicateClosestHitRayIndices) {
    GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(7), activePath(8)};
    const auto step = [&] {
      return GpuDiffusePathStepReference().step(
        sections, paths,
        {GpuIntersectionScenePacker().packMiss(7), GpuIntersectionScenePacker().packMiss(7)});
    };

    EXPECT_THROW(
      {
        const GpuDiffusePathStepResult result = step();
        (void)result;
      },
      std::logic_error);
  }

  TEST(GpuDiffusePathStepReference, RejectsUnexpectedClosestHitRayIndex) {
    GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(7)};
    const auto step = [&] {
      return GpuDiffusePathStepReference().step(sections, paths,
                                                {GpuIntersectionScenePacker().packMiss(99)});
    };

    EXPECT_THROW(
      {
        const GpuDiffusePathStepResult result = step();
        (void)result;
      },
      std::logic_error);
  }

  TEST(GpuDiffusePathStepReference, RejectsDuplicateActivePathRayIndices) {
    GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(7), activePath(7)};
    const auto step = [&] {
      return GpuDiffusePathStepReference().step(
        sections, paths,
        {GpuIntersectionScenePacker().packMiss(7), GpuIntersectionScenePacker().packMiss(7)});
    };

    EXPECT_THROW(
      {
        const GpuDiffusePathStepResult result = step();
        (void)result;
      },
      std::logic_error);
  }

  TEST(GpuDiffusePathStepReference, EmissiveHitAddsEmissionAndTerminatesPath) {
    Scene scene;
    auto lightCard = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    lightCard->setMaterial(std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0)));
    scene.add(lightCard);
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Emissive);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 0.75f, 0.0f};

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0), Colord(result.stepRecords[0].emittedRadiance),
                      1e-6);
    EXPECT_EQ(1u, result.metrics.emissiveHits);
  }

  TEST(GpuDiffusePathStepReference, MatteHitAddsVisibleDirectLightAndContinuation) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -2.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {activePath()}, {hitRecord(7, material)});

    ASSERT_EQ(1u, result.directLightShadowRays.size());
    ASSERT_EQ(1u, result.directLightOcclusionRecords.size());
    EXPECT_EQ(0u, result.directLightOcclusionRecords[0].occluded);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(result.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(result.pathStates[0].accumulatedRadiance), 1e-5);
    EXPECT_TRUE(gpuDiffusePathStateIsActive(result.pathStates[0]));
    EXPECT_EQ(1u, result.pathStates[0].depth);
    EXPECT_EQ(1u, result.metrics.directLightSamples);
    EXPECT_EQ(1u, result.metrics.directLightContributingSamples);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
  }

  TEST(GpuDiffusePathStepReference, MatteHitAddsCompiledSceneAmbientRadiance) {
    Scene scene;
    scene.setAmbient(Colord(0.2, 0.3, 0.4));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.5, 0.25, 1.0)));
    matte->setAmbientCoefficient(0.5);
    matte->setDiffuseCoefficient(0.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = compileGpuTracingScene(scene).sections;
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 0.75f, 0.0f};

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    ASSERT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    ASSERT_COLOR_NEAR(Colord(0.0125, 0.01875, 0.15),
                      colorFrom4(result.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(0u, result.metrics.directLightSamples);
  }

  TEST(GpuDiffusePathStepReference, FixedGpuSamplesEmitExpectedDiffuseContinuationRecord) {
    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.sampleSeed = 12347;
    path.throughput = {0.2f, 0.4f, 0.6f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 0;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)}, settings);

    ASSERT_EQ(1u, result.pathStates.size());
    const GpuDiffusePathStateRecord& next = result.pathStates[0];
    const Vector3d normal(0.0, 0.0, -1.0);
    const Vector2d bsdfSample =
      GpuSampleStream::sample2D(path.sampleSeed, path.pixelIndex, path.primarySampleIndex,
                                path.sampleDimensionBase + path.depth * path.sampleDimensionStride);
    const Vector3d expectedDirection = expectedCosineHemisphereDirection(normal, bsdfSample);
    const double expectedPdf = (normal * expectedDirection) * invPI;
    const Colord expectedPreRouletteThroughput =
      Colord(0.2, 0.4, 0.6) * Colord(0.25, 0.5, 0.75) * 0.8;
    const double roulette = GpuSampleStream::sample1D(
      GpuSampleCoordinate{path.sampleSeed, path.pixelIndex, path.primarySampleIndex,
                          path.sampleDimensionBase + 3u, /*component=*/0});
    const double expectedContinuationProbability = expectedPreRouletteThroughput.max();
    ASSERT_LT(roulette, expectedContinuationProbability);
    const Colord expectedThroughput =
      expectedPreRouletteThroughput * (1.0 / expectedContinuationProbability);

    expectFloat4Near(next.ray.direction, {static_cast<float>(expectedDirection.x()),
                                          static_cast<float>(expectedDirection.y()),
                                          static_cast<float>(expectedDirection.z()), 0.0f});
    EXPECT_EQ(1u, next.depth);
    EXPECT_FLOAT_EQ(static_cast<float>(expectedPdf), next.previousBsdfPdf);
    EXPECT_FLOAT_EQ(0.0f, next.previousLightPdf);
    EXPECT_EQ(material, next.previousMaterial);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag, next.previousEventFlags);
    EXPECT_TRUE(gpuDiffusePathStateIsActive(next));
    EXPECT_EQ(0u, next.flags & gpuDiffusePathStateTerminatedFlag);
    ASSERT_COLOR_NEAR(expectedThroughput, Colord(next.throughput), 1e-6);
    ASSERT_COLOR_NEAR(expectedThroughput, Colord(result.stepRecords[0].continuationThroughput),
                      1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesUvCheckerTexture) {
    auto checker = std::make_shared<CheckerBoardTexture>(
      new UVMapping2D(2.0, 2.0), std::make_shared<ConstantColorTexture>(Colord::red()),
      std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto matte = std::make_shared<MatteMaterial>(checker);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord brightPath = activePath(17);
    GpuDiffusePathStateRecord darkPath = activePath(18);
    GpuIntersectionHitRecord brightHit = hitRecord(17, material);
    brightHit.uv = {0.25f, 0.25f, 0.0f, 0.0f};
    GpuIntersectionHitRecord darkHit = hitRecord(18, material);
    darkHit.uv = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {brightPath, darkPath}, {brightHit, darkHit}, settings);

    ASSERT_EQ(2u, result.pathStates.size());
    ASSERT_COLOR_NEAR(Colord::red(), colorFrom4(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::blue(), colorFrom4(result.pathStates[1].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::red(), colorFrom4(result.stepRecords[0].continuationThroughput),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord::blue(), colorFrom4(result.stepRecords[1].continuationThroughput),
                      1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesNearestImageTexture) {
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image =
      std::make_shared<ImageTexture>(new UVMapping2D, 2, 2, pixels, ImageTextureFilter::Nearest);
    auto matte = std::make_shared<MatteMaterial>(image);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord redPath = activePath(17);
    GpuDiffusePathStateRecord greenPath = activePath(18);
    GpuDiffusePathStateRecord bluePath = activePath(19);
    GpuIntersectionHitRecord redHit = hitRecord(17, material);
    redHit.uv = {0.25f, 0.25f, 0.0f, 0.0f};
    GpuIntersectionHitRecord greenHit = hitRecord(18, material);
    greenHit.uv = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuIntersectionHitRecord blueHit = hitRecord(19, material);
    blueHit.uv = {0.25f, 0.75f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {redPath, greenPath, bluePath}, {redHit, greenHit, blueHit}, settings);

    ASSERT_EQ(3u, result.pathStates.size());
    ASSERT_COLOR_NEAR(Colord::red(), colorFrom4(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::green(), colorFrom4(result.pathStates[1].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::blue(), colorFrom4(result.pathStates[2].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::red(), colorFrom4(result.stepRecords[0].continuationThroughput),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord::green(), colorFrom4(result.stepRecords[1].continuationThroughput),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord::blue(), colorFrom4(result.stepRecords[2].continuationThroughput),
                      1e-6);
  }

  TEST(GpuDiffusePathStepReference, DirectLightOcclusionSuppressesContribution) {
    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.8, 0.4, 0.2)));
    auto receiver = std::make_shared<Sphere>(Vector3d(10.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    auto blocker = std::make_shared<Sphere>(Vector3d(0.0, 0.0, -1.0), 0.25);
    blocker->setMaterial(matte);
    scene.add(receiver);
    scene.add(blocker);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -2.0), Colord::white()));
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {activePath()}, {hitRecord(7, material)});

    ASSERT_EQ(1u, result.directLightOcclusionRecords.size());
    EXPECT_EQ(1u, result.directLightOcclusionRecords[0].occluded);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.stepRecords[0].directLightRadiance), 1e-6);
    EXPECT_EQ(1u, result.metrics.directLightOccludedSamples);
  }

  TEST(GpuDiffusePathStepReference, FixedSeedContinuationIsRepeatableAndSeedDependent) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord first = activePath();
    GpuDiffusePathStateRecord second = first;
    second.sampleSeed += 1;

    const GpuDiffusePathStepResult firstRun =
      GpuDiffusePathStepReference().step(sections, {first}, {hitRecord(7, material)});
    const GpuDiffusePathStepResult repeatedRun =
      GpuDiffusePathStepReference().step(sections, {first}, {hitRecord(7, material)});
    const GpuDiffusePathStepResult changedSeedRun =
      GpuDiffusePathStepReference().step(sections, {second}, {hitRecord(7, material)});

    ASSERT_EQ(1u, firstRun.pathStates.size());
    EXPECT_EQ(firstRun.pathStates[0].ray.direction, repeatedRun.pathStates[0].ray.direction);
    EXPECT_EQ(firstRun.pathStates[0].throughput, repeatedRun.pathStates[0].throughput);
    EXPECT_NE(firstRun.pathStates[0].ray.direction, changedSeedRun.pathStates[0].ray.direction);
    EXPECT_NEAR(1.0, Vector3d(firstRun.pathStates[0].ray.direction).length(), 1e-5);
  }

  TEST(GpuDiffusePathStepReference, DiffuseHitBeforeRouletteDepthEmitsNextPathRecord) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.1f, 0.1f, 0.1f, 0.0f};

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    ASSERT_EQ(1u, result.pathStates.size());
    EXPECT_TRUE(result.terminatedPathStates.empty());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(result.pathStates[0]));
    EXPECT_EQ(1u, result.pathStates[0].depth);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
    EXPECT_EQ(0u, result.metrics.terminatedPaths);
    ASSERT_COLOR_NEAR(Colord(0.1, 0.1, 0.1), Colord(result.pathStates[0].throughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, RouletteTerminatedDiffuseHitEmitsNoNextPathRecord) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.depth = 3;
    path.throughput = {0.1f, 0.1f, 0.1f, 0.0f};

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    EXPECT_EQ(0u, result.metrics.spawnedContinuations);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
    EXPECT_NE(0u, result.stepRecords[0].flags & gpuDiffusePathStateTerminatedFlag);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.stepRecords[0].continuationThroughput),
                      1e-6);
  }

  TEST(GpuDiffusePathLoop, ResolvesMissedPathsIntoImage) {
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.75, 0.5, 0.25));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 4;
    const GpuDiffusePathLoopResult result = GpuDiffusePathLoop().run(sections, {path}, settings);

    EXPECT_EQ(1u, result.initialPathCount);
    EXPECT_EQ(1u, result.depthCount);
    ASSERT_EQ(1u, result.activePathsPerDepth.size());
    EXPECT_EQ(1u, result.activePathsPerDepth[0]);
    EXPECT_EQ(1u, result.peakActivePathCount());
    EXPECT_EQ(1u, result.lastActivePathCount());
    ASSERT_EQ(1u, result.resolvedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      Colord(result.resolvedPathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(1u, result.metrics.misses);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);

    Buffer<unsigned int> resolved(1, 1);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(1, 1);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    EXPECT_EQ(Colord(0.125, 0.125, 0.09375).rgb(), resolved[0][0]);
    EXPECT_EQ("gpu_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("resident_accumulation_resolve", diagnostics.residency);
    EXPECT_EQ(layout.totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(1u, diagnostics.addOperations);
    EXPECT_EQ(1u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(layout.resolveBytes(), diagnostics.readbackBytes);
  }

  TEST(GpuDiffusePathLoopResult, ReportsEmptyActivePathShapeAsZero) {
    const GpuDiffusePathLoopResult result;

    EXPECT_EQ(0u, result.peakActivePathCount());
    EXPECT_EQ(0u, result.lastActivePathCount());
    EXPECT_EQ(0u, result.compactionPassCount());
    EXPECT_EQ(0u, result.inputPathCount());
    EXPECT_EQ(0u, result.retainedPathCount());
    EXPECT_EQ(0u, result.removedPathCount());
    EXPECT_EQ(0u, result.submittedIntersectionRayCount());
    EXPECT_FALSE(result.fullGpuPathLoopSupported());
    EXPECT_TRUE(result.fullGpuPathLoopUnavailable());
    EXPECT_FALSE(result.hasPlatformAccumulation());
  }

  TEST(GpuDiffusePathLoopResult, ReportsResidentPathLoopCounts) {
    GpuDiffusePathLoopResult result;
    result.depthCount = 3;
    result.activePathsPerDepth = {4, 7, 2};
    result.metrics.activePaths = 13;
    result.metrics.closestHitRays = 9;
    result.metrics.directLightVisibilityRays = 6;
    result.metrics.spawnedContinuations = 5;
    result.resolvedPathStates.resize(8);

    EXPECT_EQ(7u, result.peakActivePathCount());
    EXPECT_EQ(2u, result.lastActivePathCount());
    EXPECT_EQ(3u, result.compactionPassCount());
    EXPECT_EQ(13u, result.inputPathCount());
    EXPECT_EQ(5u, result.retainedPathCount());
    EXPECT_EQ(8u, result.removedPathCount());
    EXPECT_EQ(15u, result.submittedIntersectionRayCount());
  }

  TEST(GpuDiffusePathLoopResult, SaturatesSubmittedIntersectionRayCount) {
    GpuDiffusePathLoopResult result;
    result.metrics.closestHitRays = std::numeric_limits<std::uint64_t>::max() - 2u;
    result.metrics.directLightVisibilityRays = 8u;

    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), result.submittedIntersectionRayCount());
  }

  TEST(GpuDiffusePathLoopResult, ReportsFullGpuPathLoopAvailabilityFromExecutionPath) {
    GpuDiffusePathLoopResult result;

    EXPECT_FALSE(result.fullGpuPathLoopSupported());
    EXPECT_TRUE(result.fullGpuPathLoopUnavailable());
    EXPECT_EQ("none", result.platformLabel());

    result.executionPath = "full_gpu_subset";

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_FALSE(result.fullGpuPathLoopUnavailable());
    EXPECT_EQ("platform_gpu_path_loop", result.platformLabel());

    result.platformName = "metal";

    EXPECT_EQ("metal", result.platformLabel());
  }

  TEST(GpuDiffusePathLoopBackend, DefaultFullGpuSceneSupportFollowsBackendAvailability) {
    const GpuDiffusePathLoopBackendSupport unavailableSupport =
      CpuReferenceGpuDiffusePathLoopBackend::sharedInstance()->fullGpuPathLoopSupport({});

    EXPECT_FALSE(unavailableSupport.supported);
    EXPECT_EQ("platform full-GPU path-loop kernel is not available yet", unavailableSupport.reason);

    const AvailableFullGpuPathLoopBackend backend;
    const GpuDiffusePathLoopBackendSupport availableSupport = backend.fullGpuPathLoopSupport({});

    EXPECT_TRUE(availableSupport.supported);
    EXPECT_TRUE(availableSupport.reason.empty());
  }

  TEST(GpuDiffusePathLoopBackend, AvailableFullGpuBackendCanRejectSpecificScene) {
    const SceneRejectingFullGpuPathLoopBackend backend;

    ASSERT_TRUE(backend.fullGpuPathLoopAvailable());
    const GpuDiffusePathLoopBackendSupport support = backend.fullGpuPathLoopSupport({});

    EXPECT_FALSE(support.supported);
    EXPECT_EQ("test backend supports only a narrower scene subset", support.reason);
  }

  TEST(GpuDiffusePathLoopResult, ReportsCpuReferenceTracingCapabilitiesAsGpuFallbacks) {
    GpuDiffusePathLoopResult result;
    result.metrics.closestHitExecutionPath = "packed_cpu";
    result.metrics.directLightVisibilityExecutionPath = "packed_cpu";
    result.metrics.directLightContributionExecutionPath = "cpu_record";
    result.pathStateResidency = "cpu_host";
    TracingAccumulationDiagnostics accumulation;
    accumulation.backend = "gpu_diffuse_path_loop";
    accumulation.residency = "resident_accumulation_resolve";

    const TracingExecutionCapabilityRecords capabilities = result.tracingCapabilities(accumulation);

    EXPECT_TRUE(capabilities.hasFallback());
    EXPECT_EQ(TracingCapabilitySupport::Fallback, capabilities.intersection.closestHit.support);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.intersection.closestHit.requestedDevice);
    EXPECT_EQ(TracingExecutionDevice::CPU, capabilities.intersection.closestHit.resolvedDevice);
    EXPECT_EQ("packed_cpu", capabilities.intersection.closestHit.executionPath);
    EXPECT_EQ("platform full-GPU path-loop kernel is not available yet",
              capabilities.intersection.closestHit.fallback.reason);

    EXPECT_EQ(TracingCapabilitySupport::Restricted, capabilities.scene.materialRecords.support);
    EXPECT_EQ(TracingExecutionDevice::CPU, capabilities.scene.materialRecords.resolvedDevice);
    EXPECT_EQ("host_records", capabilities.scene.materialRecords.executionPath);

    EXPECT_EQ(TracingCapabilitySupport::Fallback, capabilities.bsdf.sample.support);
    EXPECT_EQ("compiled CPU-reference path loop samples diffuse BSDF continuations on the host",
              capabilities.bsdf.sample.fallback.reason);
    EXPECT_EQ(TracingCapabilitySupport::Fallback,
              capabilities.pathState.frontierCompaction.support);
    EXPECT_EQ("cpu_diffuse_frontier_compaction",
              capabilities.pathState.frontierCompaction.executionPath);
    EXPECT_EQ("compiled CPU-reference path loop compacts path state on the host",
              capabilities.pathState.frontierCompaction.fallback.reason);
    EXPECT_EQ(TracingCapabilitySupport::Fallback,
              capabilities.accumulation.sampleAccumulation.support);
    EXPECT_EQ("gpu_diffuse_path_loop", capabilities.accumulation.sampleAccumulation.executionPath);
    EXPECT_EQ(TracingCapabilitySupport::Supported,
              capabilities.accumulation.progressiveReadback.support);
    EXPECT_EQ(TracingExecutionDevice::CPU,
              capabilities.accumulation.progressiveReadback.resolvedDevice);
  }

  TEST(GpuDiffusePathLoopResult, ReportsGpuFrontierCompactionWithoutFullGpuPathLoop) {
    GpuDiffusePathLoopResult result;
    result.metrics.closestHitExecutionPath = "packed_cpu";
    result.metrics.directLightVisibilityExecutionPath = "packed_cpu";
    result.metrics.directLightContributionExecutionPath = "cpu_record";
    result.pathStateResidency = "cpu_host";
    result.frontierCompactionExecutionPath = "metal_diffuse_frontier_compaction";
    result.frontierCompactionPathStateResidency = "metal_shared_diffuse_path_state";
    TracingAccumulationDiagnostics accumulation;
    accumulation.backend = "gpu_diffuse_path_loop";
    accumulation.residency = "resident_accumulation_resolve";

    const TracingExecutionCapabilityRecords capabilities = result.tracingCapabilities(accumulation);

    EXPECT_TRUE(capabilities.hasFallback());
    EXPECT_EQ(TracingCapabilitySupport::Fallback, capabilities.pathState.residency.support);
    EXPECT_EQ("compiled CPU-reference path loop keeps path state on the host",
              capabilities.pathState.residency.fallback.reason);
    EXPECT_EQ(TracingCapabilitySupport::Supported,
              capabilities.pathState.frontierCompaction.support);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.pathState.frontierCompaction.requestedDevice);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.pathState.frontierCompaction.resolvedDevice);
    EXPECT_EQ("metal_diffuse_frontier_compaction",
              capabilities.pathState.frontierCompaction.executionPath);
    EXPECT_TRUE(capabilities.pathState.frontierCompaction.fallback.reason.empty());
  }

  TEST(GpuDiffusePathLoopResult, ReportsFullGpuTracingCapabilitiesWithoutCpuFallbacks) {
    GpuDiffusePathLoopResult result;
    result.executionPath = "full_gpu_subset";
    result.platformName = "metal";
    result.pathStateResidency = "metal_path_state";
    result.frontierCompactionExecutionPath = "metal_path_loop";
    result.frontierCompactionPathStateResidency = "metal_path_state";
    result.metrics.closestHitExecutionPath = "metal";
    result.metrics.directLightVisibilityExecutionPath = "metal";
    result.metrics.directLightContributionExecutionPath = "metal_path_loop";
    TracingAccumulationDiagnostics accumulation;
    accumulation.backend = "metal_accumulation";
    accumulation.residency = "metal_shared_accumulation";

    const TracingExecutionCapabilityRecords capabilities = result.tracingCapabilities(accumulation);

    EXPECT_FALSE(capabilities.hasFallback());
    EXPECT_EQ(TracingCapabilitySupport::Supported, capabilities.intersection.closestHit.support);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.intersection.closestHit.requestedDevice);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.intersection.closestHit.resolvedDevice);
    EXPECT_EQ("metal", capabilities.intersection.closestHit.platform);
    EXPECT_EQ("metal", capabilities.intersection.closestHit.executionPath);

    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.scene.geometryRecords.resolvedDevice);
    EXPECT_EQ("gpu_tracing_scene_records", capabilities.scene.geometryRecords.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.sampling.gpuRng.resolvedDevice);
    EXPECT_EQ("gpu_sample_stream", capabilities.sampling.gpuRng.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.directLighting.residentBatch.resolvedDevice);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.bsdf.eval.resolvedDevice);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.bsdf.deltaBranches.resolvedDevice);
    EXPECT_EQ(TracingCapabilitySupport::Supported, capabilities.bsdf.deltaBranches.support);
    EXPECT_EQ("full_gpu_subset", capabilities.bsdf.deltaBranches.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.pathState.residency.resolvedDevice);
    EXPECT_EQ("metal_path_state", capabilities.pathState.residency.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.pathState.frontierCompaction.resolvedDevice);
    EXPECT_EQ("metal_path_loop", capabilities.pathState.frontierCompaction.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.accumulation.sampleAccumulation.resolvedDevice);
    EXPECT_EQ("metal_accumulation", capabilities.accumulation.sampleAccumulation.executionPath);
    EXPECT_EQ(TracingExecutionDevice::Hybrid,
              capabilities.accumulation.progressiveReadback.resolvedDevice);
    EXPECT_EQ("metal_shared_accumulation",
              capabilities.accumulation.progressiveReadback.executionPath);
  }

  TEST(GpuDiffusePathLoop, ReportsCpuReferenceResidencyAndCompactionDiagnostics) {
    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.25, 0.5, 0.75));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;

    const GpuDiffusePathLoopResult result = GpuDiffusePathLoop().run(sections, {path});
    const std::uint64_t pathStateBytes = sizeof(GpuDiffusePathStateRecord);

    EXPECT_EQ("compiled_cpu_reference", result.executionPath);
    EXPECT_EQ("cpu_host", result.pathStateResidency);
    EXPECT_EQ("cpu_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
    EXPECT_EQ("cpu_host", result.frontierCompactionPathStateResidency);
    EXPECT_EQ(pathStateBytes, result.pathStateBytesPerPath());
    EXPECT_EQ(pathStateBytes, result.residentPathStateBytes());
    EXPECT_EQ(pathStateBytes, result.inputPathStateBytes());
    EXPECT_EQ(0u, result.retainedPathStateBytes());
    EXPECT_EQ(pathStateBytes, result.removedPathStateBytes());
    EXPECT_EQ(0u, result.retainedPathIndexBytes());
    EXPECT_EQ(1u, result.compactionPassCount());
    EXPECT_EQ(1u, result.inputPathCount());
    EXPECT_EQ(0u, result.retainedPathCount());
    EXPECT_EQ(1u, result.removedPathCount());
    EXPECT_EQ(1u, result.submittedIntersectionRayCount());
    EXPECT_EQ(0u, result.movedPathCount());
    EXPECT_DOUBLE_EQ(1.0, result.removedPathFraction());
    EXPECT_DOUBLE_EQ(0.0, result.movedRetainedPathFraction());
    EXPECT_EQ(1u, result.roundTrips);
    EXPECT_EQ(0u, result.savedHostReadbacks);
    EXPECT_EQ(0u, result.savedHostReadbackBytes);
  }

  TEST(CpuReferenceGpuDiffusePathFrontierCompactionBackend, CompactsRetainedPathsInOrder) {
    std::vector<GpuDiffusePathStateRecord> source{activePath(2), activePath(3), activePath(4)};
    source[0].pixelIndex = 10;
    source[1].pixelIndex = 11;
    source[2].pixelIndex = 12;

    const GpuDiffusePathFrontierCompactionResult result =
      CpuReferenceGpuDiffusePathFrontierCompactionBackend::instance().compact(source, {0u, 2u});

    EXPECT_EQ("cpu_diffuse_frontier_compaction", result.executionPath);
    EXPECT_EQ("cpu_host", result.pathStateResidency);
    EXPECT_EQ(3u, result.inputPathCount);
    EXPECT_EQ(2u, result.retainedPathCount());
    EXPECT_EQ(1u, result.removedPathCount());
    EXPECT_EQ(1u, result.movedPathCount());
    EXPECT_EQ(2u * sizeof(std::uint32_t), result.retainedIndexBytes());
    ASSERT_EQ(2u, result.retainedRecords.size());
    EXPECT_EQ(10u, result.retainedRecords[0].pixelIndex);
    EXPECT_EQ(12u, result.retainedRecords[1].pixelIndex);
  }

  TEST(MetalGpuDiffusePathFrontierCompactionBackend, CompactsRetainedPathsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathFrontierCompactionBackend backend;
    if (!backend.compactionPathAvailable()) {
      GTEST_SKIP() << backend.compactionPathUnavailableReason();
    }

    std::vector<GpuDiffusePathStateRecord> source{activePath(20), activePath(21), activePath(22)};
    source[0].pixelIndex = 100;
    source[1].pixelIndex = 101;
    source[2].pixelIndex = 102;
    source[1].sampleSeed = 9001;
    source[2].depth = 3;
    source[2].previousBsdfPdf = 0.25f;

    const GpuDiffusePathFrontierCompactionResult result = backend.compact(source, {1u, 2u});

    EXPECT_EQ("metal_diffuse_frontier_compaction", result.executionPath);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    EXPECT_EQ(3u, result.inputPathCount);
    ASSERT_EQ(2u, result.retainedRecords.size());
    EXPECT_EQ(101u, result.retainedRecords[0].pixelIndex);
    EXPECT_EQ(9001u, result.retainedRecords[0].sampleSeed);
    EXPECT_EQ(21u, result.retainedRecords[0].ray.rayIndex);
    EXPECT_EQ(102u, result.retainedRecords[1].pixelIndex);
    EXPECT_EQ(3u, result.retainedRecords[1].depth);
    EXPECT_FLOAT_EQ(0.25f, result.retainedRecords[1].previousBsdfPdf);
    EXPECT_GT(result.uploadWorkerSeconds, 0.0);
    EXPECT_GT(result.kernelWorkerSeconds, 0.0);
    EXPECT_GT(result.readbackWorkerSeconds, 0.0);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathFrontierCompactionBackend, CompactsRetainedPathsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanGpuDiffusePathFrontierCompactionBackend backend;
    if (!backend.compactionPathAvailable()) {
      GTEST_SKIP() << backend.compactionPathUnavailableReason();
    }

    std::vector<GpuDiffusePathStateRecord> source{activePath(30), activePath(31), activePath(32)};
    source[0].pixelIndex = 200;
    source[1].pixelIndex = 201;
    source[2].pixelIndex = 202;
    source[1].sampleSeed = 7001;
    source[2].depth = 4;
    source[2].previousLightPdf = 0.125f;

    const GpuDiffusePathFrontierCompactionResult result = backend.compact(source, {1u, 2u});

    EXPECT_EQ("vulkan_diffuse_frontier_compaction", result.executionPath);
    EXPECT_EQ("vulkan_host_visible_diffuse_path_state", result.pathStateResidency);
    EXPECT_EQ(3u, result.inputPathCount);
    ASSERT_EQ(2u, result.retainedRecords.size());
    EXPECT_EQ(201u, result.retainedRecords[0].pixelIndex);
    EXPECT_EQ(7001u, result.retainedRecords[0].sampleSeed);
    EXPECT_EQ(31u, result.retainedRecords[0].ray.rayIndex);
    EXPECT_EQ(202u, result.retainedRecords[1].pixelIndex);
    EXPECT_EQ(4u, result.retainedRecords[1].depth);
    EXPECT_FLOAT_EQ(0.125f, result.retainedRecords[1].previousLightPdf);
    EXPECT_GT(result.uploadWorkerSeconds, 0.0);
    EXPECT_GT(result.kernelWorkerSeconds, 0.0);
    EXPECT_GT(result.readbackWorkerSeconds, 0.0);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(GpuDiffusePathLoop, DispatchesSurvivingFrontierThroughCompactionBackend) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;

    RecordingFrontierCompactionBackend backend;
    const GpuDiffusePathLoopResult result =
      GpuDiffusePathLoop().run(sections, {path}, settings, backend);

    EXPECT_EQ(2, backend.calls);
    ASSERT_EQ(2u, backend.inputCounts.size());
    EXPECT_EQ(1u, backend.inputCounts[0]);
    EXPECT_EQ(0u, backend.inputCounts[1]);
    ASSERT_EQ(2u, backend.retainedIndices.size());
    EXPECT_EQ(std::vector<std::uint32_t>({0u}), backend.retainedIndices[0]);
    EXPECT_TRUE(backend.retainedIndices[1].empty());
    EXPECT_EQ("recording_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
    EXPECT_EQ("recording_path_state", result.frontierCompactionPathStateResidency);
    EXPECT_EQ(sizeof(std::uint32_t), result.retainedPathIndexBytes());
    EXPECT_EQ(2u, result.compactionPassCount());
    EXPECT_DOUBLE_EQ(0.02, result.frontierCompactionUploadWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.04, result.frontierCompactionKernelWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.06, result.frontierCompactionReadbackWorkerSeconds);
  }

  TEST(CompactingGpuDiffusePathLoopBackend, DispatchesLoopThroughInjectedCompactionBackend) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    auto compactionBackend = std::make_shared<RecordingFrontierCompactionBackend>();
    const CompactingGpuDiffusePathLoopBackend backend(compactionBackend);

    const GpuDiffusePathLoopResult result = backend.run(sections, {activePath()}, settings);

    EXPECT_EQ("compiled_cpu_reference_with_compaction_backend", std::string(backend.name()));
    EXPECT_EQ("recording_diffuse_frontier_compaction",
              std::string(backend.compactionBackend().name()));
    EXPECT_EQ(2, compactionBackend->calls);
    EXPECT_EQ("compiled_cpu_reference", result.executionPath);
    EXPECT_EQ("cpu_host", result.pathStateResidency);
    EXPECT_EQ("recording_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
    EXPECT_EQ("recording_path_state", result.frontierCompactionPathStateResidency);
    EXPECT_DOUBLE_EQ(0.02, result.frontierCompactionUploadWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.04, result.frontierCompactionKernelWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.06, result.frontierCompactionReadbackWorkerSeconds);
  }

  TEST(GpuDiffusePathLoopBackend, DefaultGpuRequestBackendRunsCompiledLoop) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    const std::shared_ptr<const GpuDiffusePathLoopBackend> backend =
      GpuDiffusePathLoopBackend::defaultBackendForGpuRequest();

    ASSERT_NE(nullptr, backend);
    EXPECT_FALSE(backend->fullGpuPathLoopAvailable());
    EXPECT_STREQ("platform full-GPU path-loop kernel is not available yet",
                 backend->fullGpuPathLoopUnavailableReason());
    EXPECT_STREQ("", backend->platformName());
    const GpuDiffusePathLoopResult result = backend->run(sections, {activePath()}, settings);

    EXPECT_EQ("compiled_cpu_reference", result.executionPath);
    EXPECT_FALSE(result.frontierCompactionExecutionPath.empty());
    EXPECT_FALSE(result.frontierCompactionPathStateResidency.empty());
    if (std::string(backend->name()) == "compiled_cpu_reference") {
      EXPECT_EQ("cpu_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
      EXPECT_EQ("cpu_host", result.frontierCompactionPathStateResidency);
    } else {
      EXPECT_EQ("compiled_cpu_reference_with_compaction_backend", std::string(backend->name()));
      EXPECT_NE("cpu_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
      EXPECT_NE("cpu_host", result.frontierCompactionPathStateResidency);
    }
  }

  TEST(GpuDiffusePathLoopBackend, DefaultFullGpuBackendNamesPlatformBackendWhenBuilt) {
    const std::shared_ptr<const GpuDiffusePathLoopBackend> backend =
      GpuDiffusePathLoopBackend::defaultFullGpuBackendForGpuRequest();
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("metal_diffuse_path_loop", backend->name());
    EXPECT_STREQ("metal", backend->platformName());
#elif defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("vulkan_diffuse_path_loop", backend->name());
    EXPECT_STREQ("vulkan", backend->platformName());
#else
    EXPECT_EQ(nullptr, backend);
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, ReportsUnavailableWhenBuildOrDeviceCannotRunMetal) {
    const MetalGpuDiffusePathLoopBackend backend;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const GpuDiffusePathLoopBackendSupport support =
      backend.fullGpuPathLoopSupport(GpuTracingSceneSections(), settings);

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    if (backend.fullGpuPathLoopAvailable()) {
      EXPECT_TRUE(support.supported);
      EXPECT_TRUE(support.reason.empty());
    } else {
      EXPECT_FALSE(support.supported);
      EXPECT_FALSE(support.reason.empty());
    }
#else
    EXPECT_FALSE(backend.fullGpuPathLoopAvailable());
    EXPECT_FALSE(support.supported);
    EXPECT_EQ("Metal diffuse path-loop backend is not enabled in this build", support.reason);
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, SupportsMultiDepthSettingsForRestrictedSceneWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;

    const GpuDiffusePathLoopBackendSupport support =
      backend.fullGpuPathLoopSupport(sections, settings);

    EXPECT_TRUE(support.supported);
    EXPECT_TRUE(support.reason.empty());

    Scene unsupportedMaterialScene;
    auto unsupportedMaterialSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    unsupportedMaterialSphere->setMaterial(std::make_shared<UnsupportedGpuTracingMaterial>());
    unsupportedMaterialScene.add(unsupportedMaterialSphere);
    const GpuTracingSceneSections unsupportedMaterialSections =
      sectionsFor(unsupportedMaterialScene);
    const GpuDiffusePathLoopBackendSupport unsupportedMaterialSupport =
      backend.fullGpuPathLoopSupport(unsupportedMaterialSections, settings);
    EXPECT_FALSE(unsupportedMaterialSupport.supported);
    EXPECT_EQ("Metal diffuse path-loop backend currently supports Matte, Phong finite glossy, "
              "Reflective mirror, Transparent refraction, and Emissive materials only",
              unsupportedMaterialSupport.reason);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsOneDepthAllMissPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsDuplicatePixelSamplesWithSampleSlotAccumulation) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 0;
    paths[0].primarySampleIndex = 0;
    paths[0].throughput = {1.0f, 1.0f, 1.0f, 0.0f};
    paths[1].pixelIndex = 0;
    paths[1].primarySampleIndex = 1;
    paths[1].throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
    ASSERT_EQ(paths.size(), result.platformAccumulationColorSums.size());
    ASSERT_EQ(paths.size(), result.platformAccumulationSampleCounts.size());
    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetSampleSlot,
              result.platformAccumulationTargetMode);
    EXPECT_EQ(1u, result.platformAccumulationWidth);
    EXPECT_EQ(2u, result.platformAccumulationHeight);
    EXPECT_EQ(1u, result.platformAccumulationSampleCounts[0]);
    EXPECT_EQ(1u, result.platformAccumulationSampleCounts[1]);

    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(1, 1);
    Buffer<Colord> expectedResolved(1, 1);
    Buffer<Colord> resolved(1, 1);
    (void)resolveGpuDiffusePathLoopImage(expected, layout, expectedResolved);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(expectedResolved[0][0], resolved[0][0], 1e-4);
    EXPECT_EQ("metal_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("metal_accumulation_buffer", diagnostics.residency);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, ReportsUnavailableWhenBuildOrDeviceCannotRunVulkan) {
    const VulkanGpuDiffusePathLoopBackend backend;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const GpuDiffusePathLoopBackendSupport support =
      backend.fullGpuPathLoopSupport(GpuTracingSceneSections(), settings);

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (backend.fullGpuPathLoopAvailable()) {
      EXPECT_TRUE(support.supported);
      EXPECT_TRUE(support.reason.empty());
    } else {
      EXPECT_FALSE(support.supported);
      EXPECT_FALSE(support.reason.empty());
    }
#else
    EXPECT_FALSE(backend.fullGpuPathLoopAvailable());
    EXPECT_FALSE(support.supported);
    EXPECT_EQ("Vulkan diffuse path-loop backend is not enabled in this build", support.reason);
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, SupportsEmptyAndSingleSphereGeometryWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;

    Scene emptyScene;
    const GpuTracingSceneSections emptySections = sectionsFor(emptyScene);
    const GpuDiffusePathLoopBackendSupport emptySupport =
      backend.fullGpuPathLoopSupport(emptySections, settings);
    EXPECT_TRUE(emptySupport.supported);
    EXPECT_TRUE(emptySupport.reason.empty());

    Scene sphereScene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(matte);
    sphereScene.add(sphere);
    const GpuTracingSceneSections sphereSections = sectionsFor(sphereScene);
    const GpuDiffusePathLoopBackendSupport sphereSupport =
      backend.fullGpuPathLoopSupport(sphereSections, settings);
    EXPECT_TRUE(sphereSupport.supported);
    EXPECT_TRUE(sphereSupport.reason.empty());

    Scene unsupportedScene;
    unsupportedScene.add(std::make_shared<Plane>(Vector3d(0.0, 0.0, 1.0), 0.0));
    const GpuTracingSceneSections unsupportedSections = sectionsFor(unsupportedScene);
    const GpuDiffusePathLoopBackendSupport unsupportedSupport =
      backend.fullGpuPathLoopSupport(unsupportedSections, settings);
    EXPECT_FALSE(unsupportedSupport.supported);
    EXPECT_EQ("Vulkan diffuse path-loop backend currently supports empty geometry or one "
              "untransformed sphere only",
              unsupportedSupport.reason);

    Scene unsupportedMaterialScene;
    auto unsupportedMaterialSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    unsupportedMaterialSphere->setMaterial(std::make_shared<UnsupportedGpuTracingMaterial>());
    unsupportedMaterialScene.add(unsupportedMaterialSphere);
    const GpuTracingSceneSections unsupportedMaterialSections =
      sectionsFor(unsupportedMaterialScene);
    const GpuDiffusePathLoopBackendSupport unsupportedMaterialSupport =
      backend.fullGpuPathLoopSupport(unsupportedMaterialSections, settings);
    EXPECT_FALSE(unsupportedMaterialSupport.supported);
    EXPECT_EQ("Vulkan diffuse path-loop backend currently supports Matte and Emissive materials "
              "only",
              unsupportedMaterialSupport.reason);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsOneDepthAllMissPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ("vulkan_host_visible_diffuse_path_state", result.pathStateResidency);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsOneDepthSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ("vulkan_host_visible_diffuse_path_state", result.pathStateResidency);
    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsMultiDepthSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_GT(result.frontierCompactionUploadWorkerSeconds, 0.0);
    EXPECT_GT(result.frontierCompactionKernelWorkerSeconds, 0.0);
    EXPECT_GT(result.frontierCompactionReadbackWorkerSeconds, 0.0);
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsOneDepthSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsPhongGlossyPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto phong = std::make_shared<PhongMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord(0.75, 0.5, 0.25),
      16.0);
    phong->setDiffuseCoefficient(0.8);
    phong->setSpecularCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(phong);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsReflectiveMirrorPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto reflective =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::black()));
    reflective->setDiffuseCoefficient(0.0);
    reflective->setReflectionColor(Colord(0.75, 0.5, 0.25));
    reflective->setReflectionCoefficient(0.5);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(reflective);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              result.resolvedPathStates[0].previousEventFlags);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTransparentTransmissionPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto transparent = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord::black()));
    transparent->setDiffuseCoefficient(0.0);
    transparent->setSpecularCoefficient(0.0);
    transparent->setReflectionCoefficient(0.0);
    transparent->setTransmissionCoefficient(1.0);
    transparent->setRefractionIndex(1.5);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(transparent);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              result.resolvedPathStates[0].previousEventFlags);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsSphereAndPlaneDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto floorMaterial = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.6, 0.5, 0.4)));
    floorMaterial->setDiffuseCoefficient(0.9);
    auto sphereMaterial = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    sphereMaterial->setDiffuseCoefficient(0.8);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(floorMaterial);
    scene.add(floor);
    auto receiver = std::make_shared<Sphere>(Vector3d(3.0, 0.0, 0.0), 0.5);
    receiver->setMaterial(sphereMaterial);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsPlanarCheckerDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto checker = std::make_shared<CheckerBoardTexture>(
      new PlanarMapping2D, std::make_shared<ConstantColorTexture>(Colord::red()),
      std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto material = std::make_shared<MatteMaterial>(checker);
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord brightPath =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    brightPath.pixelIndex = 0;
    GpuDiffusePathStateRecord darkPath =
      activePath(Rayd(Vector4d(1.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 18);
    darkPath.pixelIndex = 1;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{brightPath, darkPath};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    expectFloat4Near(result.stepRecords[1].continuationThroughput,
                     expected.stepRecords[1].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTriangleDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.6, 0.5, 0.4)));
    material->setDiffuseCoefficient(0.9);
    auto receiver = std::make_shared<Triangle>(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, -1.0, 0.0),
                                               Vector3d(0.0, 1.0, 0.0));
    receiver->setMaterial(material);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, 3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsRectangleAndDiskDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.6, 0.5, 0.4)));
    material->setDiffuseCoefficient(0.9);
    auto rectangle = std::make_shared<Rectangle>(Vector3d(-3.0, -1.0, 0.0), Vector3d(2.0, 0.0, 0.0),
                                                 Vector3d(0.0, 2.0, 0.0));
    rectangle->setMaterial(material);
    scene.add(rectangle);
    auto disk = std::make_shared<Disk>(Vector3d(2.0, 0.0, 0.0), Vector3d(0.0, 0.0, 1.0), 0.75);
    disk->setMaterial(material);
    scene.add(disk);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, 3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord rectanglePath =
      activePath(Rayd(Vector4d(-2.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    rectanglePath.pixelIndex = 0;
    GpuDiffusePathStateRecord diskPath =
      activePath(Rayd(Vector4d(2.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 18);
    diskPath.pixelIndex = 1;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{rectanglePath, diskPath};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsOpenCylinderDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    material->setDiffuseCoefficient(0.8);
    auto cylinder = std::make_shared<OpenCylinder>(1.0, 2.0);
    cylinder->setMaterial(material);
    scene.add(cylinder);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 19);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTorusDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.7, 0.35, 0.2)));
    material->setDiffuseCoefficient(0.85);
    auto torus = std::make_shared<Torus>(1.0, 0.25);
    torus->setMaterial(material);
    scene.add(torus);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 20);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTransformedSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    material->setDiffuseCoefficient(0.8);
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(material);
    auto instance = std::make_shared<Instance>(sphere);
    instance->setMatrix(Matrix4d::translate(0.0, 0.0, 3.0));
    scene.add(instance);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, BuildsShaderFacingLaunchPlan) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    settings.russianRouletteDepth = 2;
    settings.directLightSamples = 4;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(), activePath()};

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const auto layouts = sections.sectionLayouts();

    EXPECT_EQ(gpuDiffusePathLoopLaunchLayoutVersion, plan.parameters.layoutVersion);
    EXPECT_EQ(3u, plan.parameters.maxDepth);
    EXPECT_EQ(2u, plan.parameters.russianRouletteDepth);
    EXPECT_EQ(4u, plan.parameters.directLightSamples);
    EXPECT_EQ(2u, plan.parameters.initialPathCount);
    EXPECT_EQ(3u, plan.parameters.imageWidth);
    EXPECT_EQ(2u, plan.parameters.imageHeight);
    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetPixel, plan.parameters.accumulationTargetMode);
    EXPECT_EQ(sections.materials.size(), plan.parameters.materialCount);
    EXPECT_EQ(sections.textures.size(), plan.parameters.textureCount);
    EXPECT_EQ(sections.lights.size(), plan.parameters.lightCount);
    EXPECT_EQ(sections.environment.size(), plan.parameters.environmentCount);
    EXPECT_EQ(sections.debugIds.size(), plan.parameters.debugIdCount);
    EXPECT_EQ(layouts[0].byteOffset, plan.parameters.geometryByteOffset);
    EXPECT_EQ(layouts[1].byteOffset, plan.parameters.materialByteOffset);
    EXPECT_EQ(layouts[2].byteOffset, plan.parameters.textureByteOffset);
    EXPECT_EQ(layouts[3].byteOffset, plan.parameters.lightByteOffset);
    EXPECT_EQ(layouts[4].byteOffset, plan.parameters.environmentByteOffset);
    EXPECT_EQ(layouts[5].byteOffset, plan.parameters.debugIdByteOffset);
    EXPECT_EQ(sections.uploadByteCount(), plan.parameters.sceneUploadBytes);
    EXPECT_EQ(sections.geometry.bvh.size(), plan.parameters.bvhNodeCount);
    EXPECT_EQ(sections.geometry.primitives.size(), plan.parameters.primitiveCount);
    EXPECT_EQ(sections.geometry.triangles.size(), plan.parameters.triangleCount);
    EXPECT_EQ(sections.geometry.spheres.size(), plan.parameters.sphereCount);
    EXPECT_EQ(sections.geometry.planes.size(), plan.parameters.planeCount);
    EXPECT_EQ(sections.geometry.rectangles.size(), plan.parameters.rectangleCount);
    EXPECT_EQ(sections.geometry.disks.size(), plan.parameters.diskCount);
    EXPECT_EQ(sections.geometry.openCylinders.size(), plan.parameters.openCylinderCount);
    EXPECT_EQ(sections.geometry.tori.size(), plan.parameters.torusCount);
    EXPECT_EQ(sections.geometry.transforms.size(), plan.parameters.transformCount);

    std::uint32_t geometryOffset = layouts[0].byteOffset;
    const auto expectGeometryRange = [&geometryOffset](std::uint32_t actualOffset,
                                                       std::size_t count, std::size_t recordSize) {
      EXPECT_EQ(geometryOffset, actualOffset);
      geometryOffset += static_cast<std::uint32_t>(count * recordSize);
    };
    expectGeometryRange(plan.parameters.bvhByteOffset, sections.geometry.bvh.size(),
                        sizeof(GpuIntersectionBvhNode));
    expectGeometryRange(plan.parameters.primitiveByteOffset, sections.geometry.primitives.size(),
                        sizeof(GpuIntersectionPrimitiveRecord));
    expectGeometryRange(plan.parameters.triangleByteOffset, sections.geometry.triangles.size(),
                        sizeof(GpuIntersectionTrianglePayload));
    expectGeometryRange(plan.parameters.sphereByteOffset, sections.geometry.spheres.size(),
                        sizeof(GpuIntersectionSpherePayload));
    expectGeometryRange(plan.parameters.planeByteOffset, sections.geometry.planes.size(),
                        sizeof(GpuIntersectionPlanePayload));
    expectGeometryRange(plan.parameters.rectangleByteOffset, sections.geometry.rectangles.size(),
                        sizeof(GpuIntersectionRectanglePayload));
    expectGeometryRange(plan.parameters.diskByteOffset, sections.geometry.disks.size(),
                        sizeof(GpuIntersectionDiskPayload));
    expectGeometryRange(plan.parameters.openCylinderByteOffset,
                        sections.geometry.openCylinders.size(),
                        sizeof(GpuIntersectionOpenCylinderPayload));
    expectGeometryRange(plan.parameters.torusByteOffset, sections.geometry.tori.size(),
                        sizeof(GpuIntersectionTorusPayload));
    expectGeometryRange(plan.parameters.transformByteOffset, sections.geometry.transforms.size(),
                        sizeof(GpuIntersectionTransformPayload));
    EXPECT_EQ(layouts[0].byteOffset + layouts[0].byteCount, geometryOffset);

    EXPECT_EQ(sections.uploadByteCount(), plan.buffers.sceneUploadBytes);
    EXPECT_EQ(sections.uploadBytes(), plan.sceneUpload);
    EXPECT_EQ(2u * sizeof(GpuDiffusePathStateRecord), plan.buffers.initialPathStateBytes);
    EXPECT_EQ(2u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(2u * sizeof(GpuDiffusePathStateRecord), plan.buffers.nextPathStateBytes);
    EXPECT_EQ(2u * 3u * sizeof(GpuDiffusePathStepRecord), plan.buffers.stepRecordBytes);
    EXPECT_EQ(3u * sizeof(std::uint32_t), plan.buffers.retainedIndexBytes);
    EXPECT_EQ(accumulationLayout.totalBytes(), plan.buffers.accumulationBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.initialPathStateBytes,
              plan.buffers.totalUploadBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.activePathStateBytes +
                plan.buffers.nextPathStateBytes + plan.buffers.stepRecordBytes +
                plan.buffers.retainedIndexBytes + plan.buffers.accumulationBytes,
              plan.buffers.totalResidentBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, RejectsInvalidSettingsAndLayout) {
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 0;
    EXPECT_THROW((void)GpuDiffusePathLoopLaunchPlanner().plan(
                   GpuTracingSceneSections(), {}, TracingAccumulationLayout::image(1, 1), settings),
                 std::invalid_argument);

    settings.maxDepth = 1;
    TracingAccumulationLayout invalidLayout;
    invalidLayout.width = 0;
    invalidLayout.height = 1;
    EXPECT_THROW((void)GpuDiffusePathLoopLaunchPlanner().plan(GpuTracingSceneSections(), {},
                                                              invalidLayout, settings),
                 std::invalid_argument);
  }

  TEST(MetalGpuDiffusePathLoopKernel, LaunchProbeConsumesShaderFacingPlanWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 1;
    settings.directLightSamples = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 12;
    paths[0].sampleSeed = 101;
    paths[1].pixelIndex = 13;
    paths[1].depth = 2;
    paths[1].previousBsdfPdf = 0.5f;
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runLaunchProbe(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_launch_probe", result.executionPath);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    EXPECT_EQ(plan.parameters.layoutVersion, result.echoedParameters.layoutVersion);
    EXPECT_EQ(plan.parameters.maxDepth, result.echoedParameters.maxDepth);
    EXPECT_EQ(plan.parameters.russianRouletteDepth, result.echoedParameters.russianRouletteDepth);
    EXPECT_EQ(plan.parameters.directLightSamples, result.echoedParameters.directLightSamples);
    EXPECT_EQ(plan.parameters.initialPathCount, result.echoedParameters.initialPathCount);
    EXPECT_EQ(plan.parameters.imageWidth, result.echoedParameters.imageWidth);
    EXPECT_EQ(plan.parameters.imageHeight, result.echoedParameters.imageHeight);
    EXPECT_EQ(plan.parameters.materialCount, result.echoedParameters.materialCount);
    EXPECT_EQ(plan.parameters.textureCount, result.echoedParameters.textureCount);
    EXPECT_EQ(plan.parameters.lightCount, result.echoedParameters.lightCount);
    EXPECT_EQ(plan.parameters.environmentCount, result.echoedParameters.environmentCount);
    EXPECT_EQ(plan.parameters.debugIdCount, result.echoedParameters.debugIdCount);
    EXPECT_EQ(plan.parameters.geometryByteOffset, result.echoedParameters.geometryByteOffset);
    EXPECT_EQ(plan.parameters.materialByteOffset, result.echoedParameters.materialByteOffset);
    EXPECT_EQ(plan.parameters.textureByteOffset, result.echoedParameters.textureByteOffset);
    EXPECT_EQ(plan.parameters.lightByteOffset, result.echoedParameters.lightByteOffset);
    EXPECT_EQ(plan.parameters.environmentByteOffset, result.echoedParameters.environmentByteOffset);
    EXPECT_EQ(plan.parameters.debugIdByteOffset, result.echoedParameters.debugIdByteOffset);
    EXPECT_EQ(plan.parameters.sceneUploadBytes, result.echoedParameters.sceneUploadBytes);
    EXPECT_EQ(plan.parameters.accumulationTargetMode,
              result.echoedParameters.accumulationTargetMode);
    EXPECT_EQ(plan.parameters.bvhByteOffset, result.echoedParameters.bvhByteOffset);
    EXPECT_EQ(plan.parameters.primitiveByteOffset, result.echoedParameters.primitiveByteOffset);
    EXPECT_EQ(plan.parameters.triangleByteOffset, result.echoedParameters.triangleByteOffset);
    EXPECT_EQ(plan.parameters.sphereByteOffset, result.echoedParameters.sphereByteOffset);
    EXPECT_EQ(plan.parameters.planeByteOffset, result.echoedParameters.planeByteOffset);
    EXPECT_EQ(plan.parameters.rectangleByteOffset, result.echoedParameters.rectangleByteOffset);
    EXPECT_EQ(plan.parameters.diskByteOffset, result.echoedParameters.diskByteOffset);
    EXPECT_EQ(plan.parameters.openCylinderByteOffset,
              result.echoedParameters.openCylinderByteOffset);
    EXPECT_EQ(plan.parameters.torusByteOffset, result.echoedParameters.torusByteOffset);
    EXPECT_EQ(plan.parameters.transformByteOffset, result.echoedParameters.transformByteOffset);
    EXPECT_EQ(plan.parameters.bvhNodeCount, result.echoedParameters.bvhNodeCount);
    EXPECT_EQ(plan.parameters.primitiveCount, result.echoedParameters.primitiveCount);
    EXPECT_EQ(plan.parameters.triangleCount, result.echoedParameters.triangleCount);
    EXPECT_EQ(plan.parameters.sphereCount, result.echoedParameters.sphereCount);
    EXPECT_EQ(plan.parameters.planeCount, result.echoedParameters.planeCount);
    EXPECT_EQ(plan.parameters.rectangleCount, result.echoedParameters.rectangleCount);
    EXPECT_EQ(plan.parameters.diskCount, result.echoedParameters.diskCount);
    EXPECT_EQ(plan.parameters.openCylinderCount, result.echoedParameters.openCylinderCount);
    EXPECT_EQ(plan.parameters.torusCount, result.echoedParameters.torusCount);
    EXPECT_EQ(plan.parameters.transformCount, result.echoedParameters.transformCount);
    EXPECT_EQ(plan.buffers.totalUploadBytes, result.bufferSizes.totalUploadBytes);
    EXPECT_EQ(plan.buffers.totalResidentBytes, result.bufferSizes.totalResidentBytes);
    ASSERT_EQ(paths.size(), result.copiedInitialPathStates.size());
    EXPECT_EQ(12u, result.copiedInitialPathStates[0].pixelIndex);
    EXPECT_EQ(101u, result.copiedInitialPathStates[0].sampleSeed);
    EXPECT_EQ(40u, result.copiedInitialPathStates[0].ray.rayIndex);
    EXPECT_EQ(13u, result.copiedInitialPathStates[1].pixelIndex);
    EXPECT_EQ(2u, result.copiedInitialPathStates[1].depth);
    EXPECT_EQ(41u, result.copiedInitialPathStates[1].ray.rayIndex);
    EXPECT_FLOAT_EQ(0.5f, result.copiedInitialPathStates[1].previousBsdfPdf);
    ASSERT_EQ(paths.size(), result.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive),
              result.stepRecords[0].event);
    EXPECT_EQ(0u, result.stepRecords[0].pathIndex);
    EXPECT_EQ(12u, result.stepRecords[0].pixelIndex);
    EXPECT_EQ(paths[0].primarySampleIndex, result.stepRecords[0].primarySampleIndex);
    EXPECT_EQ(paths[0].depth, result.stepRecords[0].depth);
    EXPECT_EQ(paths[0].flags, result.stepRecords[0].flags);
    EXPECT_EQ(paths[0].throughput, result.stepRecords[0].continuationThroughput);
    EXPECT_EQ(1u, result.stepRecords[1].pathIndex);
    EXPECT_EQ(13u, result.stepRecords[1].pixelIndex);
    EXPECT_EQ(2u, result.stepRecords[1].depth);
    EXPECT_EQ(paths[1].flags, result.stepRecords[1].flags);
    EXPECT_EQ(paths[1].throughput, result.stepRecords[1].continuationThroughput);
    EXPECT_EQ(std::vector<std::uint32_t>({0u, 1u}),
              sortedRetainedPathIndices(result.retainedPathIndices));
    EXPECT_GE(result.uploadWorkerSeconds, 0.0);
    EXPECT_GE(result.kernelWorkerSeconds, 0.0);
    EXPECT_GE(result.readbackWorkerSeconds, 0.0);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, AllMissProbeResolvesBackgroundAndEnvironmentWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.75, 0.5, 0.25));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41),
                                                 makeTerminatedGpuDiffusePathState()};
    paths[0].pixelIndex = 0;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    paths[1].pixelIndex = 1;
    paths[1].depth = 1;
    paths[1].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    paths[2].pixelIndex = 2;
    paths[2].primarySampleIndex = 3;
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runAllMissProbe(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_all_miss_probe", result.executionPath);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    ASSERT_EQ(paths.size(), result.resolvedPathStates.size());
    ASSERT_EQ(paths.size(), result.stepRecords.size());

    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[0]));
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[1]));
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[2]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      colorFrom4(result.resolvedPathStates[0].accumulatedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.375, 0.125, 0.03125),
                      colorFrom4(result.resolvedPathStates[1].accumulatedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), colorFrom4(result.resolvedPathStates[2].accumulatedRadiance),
                      1e-6);

    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[1].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive),
              result.stepRecords[2].event);
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), colorFrom4(result.stepRecords[0].missRadiance),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord(0.375, 0.125, 0.03125), colorFrom4(result.stepRecords[1].missRadiance),
                      1e-6);
    EXPECT_EQ(result.resolvedPathStates[0].flags, result.stepRecords[0].flags);
    EXPECT_EQ(result.resolvedPathStates[1].flags, result.stepRecords[1].flags);
    EXPECT_EQ(paths[2].flags, result.stepRecords[2].flags);
    EXPECT_TRUE(result.retainedPathIndices.empty());

    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), colorFrom4(result.accumulationColorSums[0]),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord(0.375, 0.125, 0.03125), colorFrom4(result.accumulationColorSums[1]),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), colorFrom4(result.accumulationColorSums[2]), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), colorFrom4(result.accumulationColorSums[3]), 1e-6);
    EXPECT_EQ(1u, result.accumulationSampleCounts[0]);
    EXPECT_EQ(1u, result.accumulationSampleCounts[1]);
    EXPECT_EQ(0u, result.accumulationSampleCounts[2]);
    EXPECT_EQ(0u, result.accumulationSampleCounts[3]);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, ClosestHitProbeIntersectsSphereWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40),
      activePath(Rayd(Vector4d(3.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 41),
      makeTerminatedGpuDiffusePathState()};
    paths[0].pixelIndex = 0;
    paths[1].pixelIndex = 1;
    paths[2].pixelIndex = 2;
    paths[2].ray = GpuIntersectionScenePacker().packRay(
      Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 42);
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runClosestHitProbe(plan, paths);
    const GpuIntersectionHitRecord expectedHit =
      GpuIntersectionIntersector().intersectClosest(sections.geometry, paths[0].ray);

    EXPECT_EQ("metal_diffuse_path_loop_closest_hit_probe", result.executionPath);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    ASSERT_EQ(paths.size(), result.copiedInitialPathStates.size());
    ASSERT_EQ(paths.size(), result.closestHitRecords.size());
    ASSERT_EQ(paths.size(), result.stepRecords.size());

    EXPECT_EQ(1u, result.closestHitRecords[0].hit);
    EXPECT_EQ(expectedHit.material, result.closestHitRecords[0].material);
    EXPECT_EQ(expectedHit.object, result.closestHitRecords[0].object);
    EXPECT_EQ(expectedHit.primitiveRecord, result.closestHitRecords[0].primitiveRecord);
    EXPECT_EQ(40u, result.closestHitRecords[0].rayIndex);
    EXPECT_NEAR(expectedHit.distance, result.closestHitRecords[0].distance, 1e-5f);
    expectFloat4Near(result.closestHitRecords[0].point, expectedHit.point, 1e-5f);
    expectFloat4Near(result.closestHitRecords[0].normal, expectedHit.normal, 1e-5f);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(expectedHit.material, result.stepRecords[0].material);
    EXPECT_EQ(expectedHit.object, result.stepRecords[0].object);
    EXPECT_EQ(paths[0].flags, result.stepRecords[0].flags);

    EXPECT_EQ(0u, result.closestHitRecords[1].hit);
    EXPECT_EQ(41u, result.closestHitRecords[1].rayIndex);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[1].event);
    EXPECT_EQ(paths[1].flags, result.stepRecords[1].flags);

    EXPECT_EQ(0u, result.closestHitRecords[2].hit);
    EXPECT_EQ(42u, result.closestHitRecords[2].rayIndex);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive),
              result.stepRecords[2].event);
    EXPECT_EQ(paths[2].flags, result.stepRecords[2].flags);
    EXPECT_EQ(std::vector<std::uint32_t>({0u, 1u}),
              sortedRetainedPathIndices(result.retainedPathIndices));
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, ClosestHitProbeRejectsEmptyGeometryBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runClosestHitProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, ClosestHitProbeRejectsUnsupportedGeometryBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);
    plan.parameters.sphereCount = 0;

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runClosestHitProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteHitShadingProbeComputesContinuationWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.2, 0.4, 0.8)));
    matte->setDiffuseCoefficient(0.5);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40),
      activePath(Rayd(Vector4d(3.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 41)};
    paths[0].pixelIndex = 0;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    paths[1].pixelIndex = 1;
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runMatteHitShadingProbe(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_matte_hit_shading_probe", result.executionPath);
    ASSERT_EQ(paths.size(), result.closestHitRecords.size());
    ASSERT_EQ(paths.size(), result.stepRecords.size());
    EXPECT_EQ(1u, result.closestHitRecords[0].hit);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(result.closestHitRecords[0].material, result.stepRecords[0].material);
    EXPECT_EQ(result.closestHitRecords[0].object, result.stepRecords[0].object);
    expectFloat4Near(result.stepRecords[0].continuationThroughput, {0.05f, 0.05f, 0.05f, 0.0f},
                     1e-5f);
    EXPECT_EQ(paths[0].flags, result.stepRecords[0].flags);

    EXPECT_EQ(0u, result.closestHitRecords[1].hit);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[1].event);
    EXPECT_EQ(paths[1].flags, result.stepRecords[1].flags);
    EXPECT_EQ(std::vector<std::uint32_t>({0u, 1u}),
              sortedRetainedPathIndices(result.retainedPathIndices));
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeSpawnsNextPathWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40)};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].throughput = {0.2f, 0.4f, 0.6f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_EQ(1u, expected.pathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_matte_continuation_probe", result.executionPath);
    ASSERT_EQ(paths.size(), result.copiedInitialPathStates.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    ASSERT_EQ(paths.size(), result.closestHitRecords.size());
    ASSERT_EQ(paths.size(), result.stepRecords.size());
    expectPathStateNear(result.copiedInitialPathStates[0], paths[0], 1e-5);
    expectHitRecordNear(result.closestHitRecords[0], expected.closestHitRecords[0], 1e-5);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(expected.stepRecords[0].material, result.stepRecords[0].material);
    EXPECT_EQ(expected.stepRecords[0].object, result.stepRecords[0].object);
    EXPECT_EQ(expected.pathStates[0].flags, result.stepRecords[0].flags);
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-5);
    expectPathStateNear(result.nextPathStates[0], expected.pathStates[0], 1e-4);
    EXPECT_EQ(std::vector<std::uint32_t>({0u}), result.retainedPathIndices);
    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    ASSERT_COLOR_NEAR(Colord::black(), colorFrom4(result.accumulationColorSums[0]), 1e-6);
    EXPECT_EQ(0u, result.accumulationSampleCounts[0]);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeAddsCompiledSceneAmbientWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setAmbient(Colord(0.2, 0.3, 0.4));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.5, 0.25, 1.0)));
    matte->setAmbientCoefficient(0.5);
    matte->setDiffuseCoefficient(0.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = compileGpuTracingScene(scene).sections;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40)};
    paths[0].pixelIndex = 0;
    paths[0].throughput = {0.25f, 0.5f, 0.75f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_EQ(1u, expected.terminatedPathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.nextPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(expected.terminatedPathStates[0].accumulatedRadiance),
                      colorFrom4(result.nextPathStates[0].accumulatedRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(expected.terminatedPathStates[0].accumulatedRadiance),
                      colorFrom4(result.accumulationColorSums[0]), 1e-5);
    EXPECT_EQ(1u, result.accumulationSampleCounts[0]);
    EXPECT_TRUE(result.retainedPathIndices.empty());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeResolvesMissAndAccumulatesWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(3.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40)};
    paths[0].pixelIndex = 1;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    paths[0].accumulatedRadiance = {0.1f, 0.2f, 0.3f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_TRUE(expected.pathStates.empty());
    ASSERT_EQ(1u, expected.terminatedPathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    ASSERT_EQ(paths.size(), result.stepRecords.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    expectFloat4Near(result.stepRecords[0].missRadiance, expected.stepRecords[0].missRadiance,
                     1e-5);
    expectPathStateNear(result.nextPathStates[0], expected.terminatedPathStates[0], 1e-4);
    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    ASSERT_COLOR_NEAR(Colord(expected.terminatedPathStates[0].accumulatedRadiance),
                      colorFrom4(result.accumulationColorSums[1]), 1e-5);
    ASSERT_COLOR_NEAR(Colord::black(), colorFrom4(result.accumulationColorSums[0]), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), colorFrom4(result.accumulationColorSums[2]), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), colorFrom4(result.accumulationColorSums[3]), 1e-6);
    EXPECT_EQ(0u, result.accumulationSampleCounts[0]);
    EXPECT_EQ(1u, result.accumulationSampleCounts[1]);
    EXPECT_EQ(0u, result.accumulationSampleCounts[2]);
    EXPECT_EQ(0u, result.accumulationSampleCounts[3]);
    EXPECT_TRUE(result.retainedPathIndices.empty());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeAddsPointLightContributionWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_EQ(1u, expected.pathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    ASSERT_EQ(paths.size(), result.stepRecords.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    expectFloat4Near(result.stepRecords[0].directLightRadiance,
                     expected.stepRecords[0].directLightRadiance, 1e-5);
    expectFloat4Near(result.nextPathStates[0].accumulatedRadiance,
                     expected.pathStates[0].accumulatedRadiance, 1e-5);
    expectPathStateNear(result.nextPathStates[0], expected.pathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel,
       MatteContinuationProbeAddsRectangularAreaLightContributionWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(
      std::make_shared<RectangularAreaLight>(Vector3d(0.0, 2.0, -3.0), Vector3d(2.0, 0.0, 0.0),
                                             Vector3d(0.0, 2.0, 0.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_EQ(1u, expected.pathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    ASSERT_EQ(paths.size(), result.stepRecords.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    expectFloat4Near(result.stepRecords[0].directLightRadiance,
                     expected.stepRecords[0].directLightRadiance, 1e-4);
    expectFloat4Near(result.nextPathStates[0].accumulatedRadiance,
                     expected.pathStates[0].accumulatedRadiance, 1e-4);
    expectPathStateNear(result.nextPathStates[0], expected.pathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeTerminatesEmissiveHitWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto lightCard = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    lightCard->setMaterial(std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0)));
    scene.add(lightCard);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].throughput = {0.25f, 0.5f, 0.75f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_TRUE(expected.pathStates.empty());
    ASSERT_EQ(1u, expected.terminatedPathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    ASSERT_EQ(paths.size(), result.stepRecords.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    expectFloat4Near(result.stepRecords[0].emittedRadiance, expected.stepRecords[0].emittedRadiance,
                     1e-5);
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-5);
    expectPathStateNear(result.nextPathStates[0], expected.terminatedPathStates[0], 1e-4);
    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    ASSERT_COLOR_NEAR(Colord(expected.terminatedPathStates[0].accumulatedRadiance),
                      colorFrom4(result.accumulationColorSums[0]), 1e-5);
    EXPECT_EQ(1u, result.accumulationSampleCounts[0]);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MattePathLoopRunsMultipleDepthsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runMattePathLoop(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop", result.executionPath);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    for (std::size_t index = 0; index != expected.stepRecords.size(); ++index) {
      EXPECT_EQ(expected.stepRecords[index].event, result.stepRecords[index].event);
      EXPECT_EQ(expected.stepRecords[index].depth, result.stepRecords[index].depth);
      expectFloat4Near(result.stepRecords[index].directLightRadiance,
                       expected.stepRecords[index].directLightRadiance, 1e-4);
      expectFloat4Near(result.stepRecords[index].missRadiance,
                       expected.stepRecords[index].missRadiance, 1e-4);
      expectFloat4Near(result.stepRecords[index].continuationThroughput,
                       expected.stepRecords[index].continuationThroughput, 1e-4);
    }
    ASSERT_EQ(expected.resolvedPathStates.size(), result.nextPathStates.size());
    expectPathStateNear(result.nextPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_TRUE(result.retainedPathIndices.empty());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteHitShadingProbeRejectsMissingRecordsBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);
    plan.parameters.textureCount = 0;

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runMatteHitShadingProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeRejectsMissingRecordsBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);
    plan.parameters.materialCount = 0;

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runMatteContinuationProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, AllMissProbeRejectsDuplicateAccumulationTargets) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 0;
    paths[1].pixelIndex = 0;
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 2), settings);

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runAllMissProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeRejectsDuplicateAccumulationTargets) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 0;
    paths[1].pixelIndex = 0;
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 2), settings);

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runMatteContinuationProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, AllMissProbeRejectsNonEmptyGeometryBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runAllMissProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(CompactingGpuDiffusePathLoopBackend, RejectsMissingCompactionBackend) {
    EXPECT_THROW(CompactingGpuDiffusePathLoopBackend(nullptr), std::invalid_argument);
  }

  TEST(GpuDiffusePathLoop, TerminatesSurvivingContinuationsAtMaxDepth) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const GpuDiffusePathLoopResult result =
      GpuDiffusePathLoop().run(sections, {activePath()}, settings);

    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(1u, result.resolvedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[0]));
    EXPECT_FALSE(gpuDiffusePathStateIsActive(result.resolvedPathStates[0]));
    EXPECT_EQ(1u, result.resolvedPathStates[0].depth);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);

    Buffer<unsigned int> resolved(2, 2);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, TracingAccumulationLayout::image(2, 2), resolved);
    EXPECT_EQ(1u, diagnostics.addedSamples);
  }

  TEST(GpuDiffusePathLoop, ResolvesMultipleSamplesPerPixelAndRejectsOutOfRangePixels) {
    GpuDiffusePathStateRecord first = makeTerminatedGpuDiffusePathState();
    first.pixelIndex = 0;
    first.accumulatedRadiance = {0.25f, 0.5f, 0.75f, 0.0f};
    GpuDiffusePathStateRecord second = makeTerminatedGpuDiffusePathState();
    second.pixelIndex = 0;
    second.accumulatedRadiance = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuDiffusePathStateRecord third = makeTerminatedGpuDiffusePathState();
    third.pixelIndex = 2;
    third.accumulatedRadiance = {0.25f, 0.0f, 0.5f, 0.0f};

    Buffer<unsigned int> resolved(2, 2);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage({first, second, third}, layout, resolved);

    EXPECT_EQ(Colord(0.5, 0.375, 0.375).rgb(), resolved[0][0]);
    EXPECT_EQ(Colord::black().rgb(), resolved[0][1]);
    EXPECT_EQ(Colord(0.25, 0.0, 0.5).rgb(), resolved[1][0]);
    EXPECT_EQ(Colord::black().rgb(), resolved[1][1]);
    EXPECT_EQ(3u, diagnostics.addedSamples);

    GpuDiffusePathStateRecord outOfRange = makeTerminatedGpuDiffusePathState();
    outOfRange.pixelIndex = 4;
    outOfRange.accumulatedRadiance = {1.0f, 1.0f, 1.0f, 0.0f};
    const auto resolve = [&]() {
      const TracingAccumulationDiagnostics unused =
        resolveGpuDiffusePathLoopImage({outOfRange}, layout, resolved);
      (void)unused;
    };
    EXPECT_THROW(resolve(), std::out_of_range);
  }

  TEST(GpuDiffusePathLoop, ResolvesMultipleSamplesPerPixelIntoHdrImage) {
    GpuDiffusePathStateRecord first = makeTerminatedGpuDiffusePathState();
    first.pixelIndex = 0;
    first.accumulatedRadiance = {0.25f, 0.5f, 0.75f, 0.0f};
    GpuDiffusePathStateRecord second = makeTerminatedGpuDiffusePathState();
    second.pixelIndex = 0;
    second.accumulatedRadiance = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuDiffusePathStateRecord third = makeTerminatedGpuDiffusePathState();
    third.pixelIndex = 2;
    third.accumulatedRadiance = {0.25f, 0.0f, 0.5f, 0.0f};

    Buffer<Colord> resolved(2, 2);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    GpuDiffusePathLoopResult result;
    result.resolvedPathStates = {first, second, third};
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord(0.5, 0.375, 0.375), resolved[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[0][1], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.0, 0.5), resolved[1][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[1][1], 1e-12);
    EXPECT_EQ("gpu_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("resident_accumulation_resolve", diagnostics.residency);
    EXPECT_EQ(layout.totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(3u, diagnostics.addOperations);
    EXPECT_EQ(3u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(layout.colorSumBytes(), diagnostics.readbackBytes);

    Buffer<Colord> wrongSize(1, 2);
    const auto resolveWrongSize = [&]() {
      const TracingAccumulationDiagnostics unused =
        resolveGpuDiffusePathLoopImage(result, layout, wrongSize);
      (void)unused;
    };
    EXPECT_THROW(resolveWrongSize(), std::invalid_argument);
  }

  TEST(GpuDiffusePathLoop, ResolvesMatchingPlatformAccumulationIntoHdrImage) {
    GpuDiffusePathStateRecord fallback = makeTerminatedGpuDiffusePathState();
    fallback.pixelIndex = 0;
    fallback.accumulatedRadiance = {1.0f, 0.0f, 0.0f, 0.0f};

    GpuDiffusePathLoopResult result;
    result.resolvedPathStates = {fallback};
    result.platformAccumulationBackend = "metal_diffuse_path_loop";
    result.platformAccumulationResidency = "metal_accumulation_buffer";
    result.platformAccumulationColorSums = {{{1.0f, 0.5f, 0.0f, 0.0f},
                                             {0.0f, 0.0f, 0.0f, 0.0f},
                                             {0.25f, 0.75f, 0.5f, 0.0f},
                                             {0.0f, 0.0f, 0.0f, 0.0f}}};
    result.platformAccumulationSampleCounts = {2u, 0u, 1u, 0u};

    Buffer<Colord> resolved(2, 2);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord(0.5, 0.25, 0.0), resolved[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[0][1], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.75, 0.5), resolved[1][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[1][1], 1e-12);
    EXPECT_EQ("metal_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("metal_accumulation_buffer", diagnostics.residency);
    EXPECT_EQ(layout.totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(2u, diagnostics.addOperations);
    EXPECT_EQ(3u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(layout.accumulationBytes(), diagnostics.readbackBytes);
  }

  TEST(GpuDiffusePathLoop, ResolvesSampleSlotPlatformAccumulationIntoHdrImage) {
    GpuDiffusePathStateRecord fallback = makeTerminatedGpuDiffusePathState();
    fallback.pixelIndex = 0;
    fallback.accumulatedRadiance = {1.0f, 0.0f, 0.0f, 0.0f};

    GpuDiffusePathLoopResult result;
    result.resolvedPathStates = {fallback};
    result.platformAccumulationBackend = "metal_diffuse_path_loop";
    result.platformAccumulationResidency = "metal_accumulation_buffer";
    result.platformAccumulationTargetMode = gpuDiffusePathLoopAccumulationTargetSampleSlot;
    result.platformAccumulationWidth = 2u;
    result.platformAccumulationHeight = 3u;
    result.platformAccumulationColorSums = {{{0.25f, 0.5f, 0.0f, 0.0f},
                                             {0.75f, 0.25f, 0.25f, 0.0f},
                                             {0.0f, 0.0f, 0.0f, 0.0f},
                                             {0.0f, 0.25f, 0.5f, 0.0f},
                                             {0.5f, 0.0f, 0.25f, 0.0f},
                                             {1.0f, 0.75f, 0.0f, 0.0f}}};
    result.platformAccumulationSampleCounts = {1u, 1u, 0u, 1u, 1u, 1u};

    Buffer<Colord> resolved(2, 1);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 1);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord(0.5, 0.375, 0.125), resolved[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.5, 1.0 / 3.0, 0.25), resolved[0][1], 1e-12);
    EXPECT_EQ("metal_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("metal_accumulation_buffer", diagnostics.residency);
    EXPECT_EQ(TracingAccumulationLayout::image(2, 3).totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(5u, diagnostics.addOperations);
    EXPECT_EQ(5u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(TracingAccumulationLayout::image(2, 3).accumulationBytes(),
              diagnostics.readbackBytes);
  }

  TEST(GpuDiffusePathLoop, FallsBackToPathStatesWhenPlatformAccumulationShapeDiffers) {
    GpuDiffusePathStateRecord path = makeTerminatedGpuDiffusePathState();
    path.pixelIndex = 3;
    path.accumulatedRadiance = {0.25f, 0.5f, 0.75f, 0.0f};

    GpuDiffusePathLoopResult result;
    result.resolvedPathStates = {path};
    result.platformAccumulationBackend = "metal_diffuse_path_loop";
    result.platformAccumulationResidency = "metal_accumulation_buffer";
    result.platformAccumulationColorSums = {{{1.0f, 0.0f, 0.0f, 0.0f}}};
    result.platformAccumulationSampleCounts = {1u};

    Buffer<Colord> resolved(2, 2);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord::black(), resolved[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[0][1], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[1][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), resolved[1][1], 1e-12);
    EXPECT_EQ("gpu_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("resident_accumulation_resolve", diagnostics.residency);
    EXPECT_EQ(1u, diagnostics.addedSamples);
  }
}
