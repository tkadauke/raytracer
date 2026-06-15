#include <gtest/gtest.h>

#include "core/Color.h"
#include "core/math/Constants.h"
#include "test/helpers/ColorTestHelper.h"

#include "render/GpuDiffusePathStepReference.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/lights/PointLight.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

#include <limits>
#include <cmath>
#include <memory>
#include <vector>

namespace GpuDiffusePathStepReferenceTest {
  using namespace render;

  namespace {
    Colord colorFrom4(const std::array<float, 4>& value) {
      return Colord(value[0], value[1], value[2]);
    }

    Vector3d vectorFrom4(const std::array<float, 4>& value) {
      return Vector3d(value[0], value[1], value[2]);
    }

    GpuDiffusePathStateRecord activePath(std::uint32_t rayIndex = 7) {
      GpuDiffusePathStateRecord path = makeActiveGpuDiffusePathState();
      path.ray = GpuIntersectionScenePacker().packRay(
        Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), rayIndex);
      path.pixelIndex = 3;
      path.primarySampleIndex = 2;
      path.sampleSeed = 12345;
      path.sampleDimensionBase = 16;
      path.sampleDimensionStride = 4;
      return path;
    }

    GpuDiffusePathStateRecord activePath(const Rayd& ray, std::uint32_t rayIndex) {
      GpuDiffusePathStateRecord path = activePath(rayIndex);
      path.ray = GpuIntersectionScenePacker().packRay(ray, rayIndex);
      return path;
    }

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
      sections.environment.push_back(
        makeGpuTracingConstantEnvironment(scene.environmentRadiance()));
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

  TEST(GpuDiffusePathStep, UnsupportedMaterialLookupTerminatesExplicitly) {
    auto unsupportedSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    unsupportedSphere->setMaterial(
      std::make_shared<PhongMaterial>(std::make_shared<ConstantColorTexture>(Colord::white())));

    Scene scene;
    scene.add(unsupportedSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {activePath()});

    ASSERT_EQ(1u, result.closestHitRecords.size());
    EXPECT_TRUE(result.closestHitRecords[0].hit);
    ASSERT_LT(result.closestHitRecords[0].material, sections.materials.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported),
              sections.materials[result.closestHitRecords[0].material].kind);

    ASSERT_EQ(1u, result.pathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.pathStates[0]));
    EXPECT_NE(0u, result.pathStates[0].flags & gpuDiffusePathStateUnsupportedFlag);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Unsupported),
              result.stepRecords[0].event);
    EXPECT_EQ(result.closestHitRecords[0].material, result.stepRecords[0].material);
    EXPECT_EQ(1u, result.metrics.unsupportedHits);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, EmissiveHitFeedsContributionIntoPathState) {
    Scene scene;
    auto lightCard = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    lightCard->setMaterial(std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0)));
    scene.add(lightCard);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 0.75f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {path});

    ASSERT_EQ(1u, result.pathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.pathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0), colorFrom4(result.pathStates[0].accumulatedRadiance),
                      1e-5);
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0), colorFrom4(result.stepRecords[0].emittedRadiance),
                      1e-5);
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);
    EXPECT_EQ("cpu_record", result.metrics.emissionExecutionPath);
    EXPECT_EQ(1u, result.metrics.emissiveHits);
    EXPECT_EQ(1u, result.metrics.emissionContributionEvaluations);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, MatteHitFeedsDirectLightContributionIntoPathState) {
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
                      colorFrom4(result.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      colorFrom4(result.pathStates[0].accumulatedRadiance), 1e-5);
    EXPECT_TRUE(gpuDiffusePathStateIsActive(result.pathStates[0]));
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);
    EXPECT_EQ("packed_cpu", result.metrics.directLightVisibilityExecutionPath);
    EXPECT_EQ("cpu_record", result.metrics.directLightContributionExecutionPath);
    EXPECT_EQ(1u, result.metrics.directLightSamples);
    EXPECT_EQ(1u, result.metrics.directLightVisibilityRays);
    EXPECT_EQ(1u, result.metrics.directLightContributionEvaluations);
    EXPECT_EQ(1u, result.metrics.directLightContributingSamples);
  }

  TEST(GpuDiffusePathStepReference, MissAddsEnvironmentAndTerminatesPath) {
    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.25, 0.5, 0.75));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {path}, {GpuIntersectionScenePacker().packMiss(7)});

    ASSERT_EQ(1u, result.pathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.pathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      colorFrom4(result.pathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    EXPECT_EQ(1u, result.metrics.misses);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
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

    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.pathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0), colorFrom4(result.pathStates[0].accumulatedRadiance),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0), colorFrom4(result.stepRecords[0].emittedRadiance),
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
                      colorFrom4(result.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      colorFrom4(result.pathStates[0].accumulatedRadiance), 1e-5);
    EXPECT_TRUE(gpuDiffusePathStateIsActive(result.pathStates[0]));
    EXPECT_EQ(1u, result.pathStates[0].depth);
    EXPECT_EQ(1u, result.metrics.directLightSamples);
    EXPECT_EQ(1u, result.metrics.directLightContributingSamples);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
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
    ASSERT_COLOR_NEAR(Colord::black(), colorFrom4(result.stepRecords[0].directLightRadiance), 1e-6);
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
    EXPECT_NEAR(1.0, vectorFrom4(firstRun.pathStates[0].ray.direction).length(), 1e-5);
  }
}
