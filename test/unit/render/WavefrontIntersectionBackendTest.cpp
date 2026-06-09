#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/math/Matrix.h"
#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/State.h"
#include "render/VulkanWavefrontSmokeKernel.h"
#include "render/WavefrontIntersectionBackend.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalWavefrontSmokeKernel.h"
#endif
#include "render/primitives/Disk.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Triangle.h"

namespace WavefrontIntersectionBackendTest {
  using namespace render;

  namespace {
    void expectUnavailablePlatformFallback(const WavefrontIntersectionBackend& backend,
                                           const char* platformName,
                                           const char* expectedExecutionPath = "runtime_scene",
                                           const char* expectedPlatformId = nullptr) {
      EXPECT_STREQ("gpu", backend.requestedName());
      EXPECT_STREQ("cpu", backend.name());
      EXPECT_STREQ(expectedPlatformId ? expectedPlatformId : platformName, backend.platformName());
      EXPECT_STREQ("fallback", backend.availability());
      EXPECT_STREQ(expectedExecutionPath, backend.executionPath());
      EXPECT_NE(std::string::npos, std::string(backend.fallbackReason()).find(platformName));
      const std::string reason = backend.fallbackReason();
      const bool disabled = reason.find("not enabled") != std::string::npos;
      const bool enabledWithoutClosestHitKernel =
        reason.find("no render-path closest-hit kernel") != std::string::npos;
      const bool enabledWithoutBasicHitKernel =
        reason.find("no render-path basic hit kernel") != std::string::npos;
      const bool enabledWithoutDevice = reason.find("no Metal device") != std::string::npos;
      const bool enabledWithoutVulkanComputeDevice =
        reason.find("no Vulkan compute device") != std::string::npos;
      const bool notTriangleEligible =
        reason.find("not eligible for the Metal triangle") != std::string::npos;
      const bool noPreparedTriangleScene =
        reason.find("no prepared triangle scene") != std::string::npos;
      const bool notBasicEligible =
        reason.find("not eligible for the Metal basic") != std::string::npos;
      const bool noPreparedBasicScene =
        reason.find("no prepared basic-hit scene") != std::string::npos;
      EXPECT_TRUE(disabled || enabledWithoutClosestHitKernel || enabledWithoutBasicHitKernel ||
                  enabledWithoutDevice || enabledWithoutVulkanComputeDevice ||
                  notTriangleEligible || noPreparedTriangleScene || notBasicEligible ||
                  noPreparedBasicScene)
        << reason;
    }

    WavefrontIntersectionSceneDiagnostics supportedPackedDiagnostics() {
      WavefrontIntersectionSceneDiagnostics diagnostics;
      diagnostics.compiled = true;
      diagnostics.primitives = 1;
      diagnostics.triangleClosestHitKernelEligible = true;
      diagnostics.basicHitKernelEligible = true;
      diagnostics.packedClosestHitKernelEligible = true;
      diagnostics.packedAnyHitKernelEligible = true;
      return diagnostics;
    }

    bool hostPlatformIntersectionDeviceAvailable() {
#if defined(__APPLE__)
      return MetalWavefrontIntersectionBackend::instance().isAvailable();
#else
      return VulkanWavefrontIntersectionBackend::instance().isAvailable();
#endif
    }

    bool hostPlatformIntersectionRenderPathAvailable() {
#if defined(__APPLE__)
      return MetalWavefrontIntersectionBackend::instance().platformGpuRenderPathAvailable();
#else
      return VulkanWavefrontIntersectionBackend::instance().platformGpuRenderPathAvailable();
#endif
    }
  }

  TEST(WavefrontIntersectionQueryTiming, MergesExecutionPaths) {
    WavefrontIntersectionQueryTiming timing;
    WavefrontIntersectionQueryTiming packed;
    packed.uploadSeconds = 1.0;
    packed.recordExecutionPath("packed_cpu");
    timing.add(packed);

    EXPECT_DOUBLE_EQ(1.0, timing.uploadSeconds);
    EXPECT_EQ("packed_cpu", timing.executionPath);

    WavefrontIntersectionQueryTiming samePath;
    samePath.kernelSeconds = 2.0;
    samePath.recordExecutionPath("packed_cpu");
    timing.add(samePath);

    EXPECT_DOUBLE_EQ(2.0, timing.kernelSeconds);
    EXPECT_EQ("packed_cpu", timing.executionPath);

    WavefrontIntersectionQueryTiming metal;
    metal.readbackSeconds = 3.0;
    metal.recordExecutionPath("metal");
    timing.add(metal);

    EXPECT_DOUBLE_EQ(3.0, timing.readbackSeconds);
    EXPECT_EQ("mixed", timing.executionPath);
  }

  TEST(WavefrontIntersectionBackend, CpuBackendReportsRuntimeSceneExecutionPath) {
    const auto& backend = CpuWavefrontIntersectionBackend::instance();

    EXPECT_STREQ("cpu", backend.requestedName());
    EXPECT_STREQ("cpu", backend.name());
    EXPECT_STREQ("available", backend.availability());
    EXPECT_STREQ("runtime_scene", backend.executionPath());
    EXPECT_EQ(0u, backend.estimatedClosestHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend.estimatedClosestHitReadbackBytes(4));
    EXPECT_EQ(0u, backend.estimatedAnyHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend.estimatedAnyHitReadbackBytes(4));
    EXPECT_FALSE(backend.prefersClosestHitBatch(4));
    EXPECT_FALSE(backend.prefersAnyHitBatch(4));
  }

  TEST(WavefrontIntersectionBackend, CpuQueriesReportRuntimeSceneTimingPath) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    const auto& backend = CpuWavefrontIntersectionBackend::instance();

    State closestState;
    HitPointInterval hitPoints;
    WavefrontIntersectionQueryTiming closestTiming;
    const Primitive* hit =
      backend.intersectClosest(scene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), hitPoints,
                               closestState, &closestTiming);

    ASSERT_NE(nullptr, hit);
    EXPECT_EQ("runtime_scene", closestTiming.executionPath);

    State anyState;
    WavefrontIntersectionQueryTiming anyTiming;
    EXPECT_TRUE(backend.intersectAny(scene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 4.0,
                                     anyState, &anyTiming));
    EXPECT_EQ("runtime_scene", anyTiming.executionPath);
  }

  TEST(WavefrontIntersectionBackend, MetalStubReportsUnavailableCpuFallback) {
    const auto& backend = MetalWavefrontIntersectionBackend::instance();

    EXPECT_STREQ("metal", backend.platformName());
#if !defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    EXPECT_FALSE(backend.isAvailable());
    EXPECT_FALSE(backend.platformGpuRenderPathAvailable());
#else
    EXPECT_EQ(MetalWavefrontSmokeKernel().deviceAvailable(), backend.isAvailable());
    EXPECT_EQ(MetalWavefrontSmokeKernel().renderPathAvailable(),
              backend.platformGpuRenderPathAvailable());
#endif
    EXPECT_EQ(nullptr, backend.compiledScene());
    expectUnavailablePlatformFallback(backend, "Metal", "runtime_scene", "metal");
  }

  TEST(WavefrontIntersectionBackend, VulkanStubReportsUnavailableCpuFallback) {
    const auto& backend = VulkanWavefrontIntersectionBackend::instance();

    EXPECT_STREQ("vulkan", backend.platformName());
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    EXPECT_EQ(VulkanWavefrontSmokeKernel().deviceAvailable(), backend.isAvailable());
#else
    EXPECT_FALSE(backend.isAvailable());
#endif
    EXPECT_EQ(nullptr, backend.compiledScene());
    expectUnavailablePlatformFallback(backend, "Vulkan", "runtime_scene", "vulkan");
  }

  TEST(VulkanWavefrontSmokeKernel, ReportsUnavailableWhenDisabled) {
    VulkanWavefrontSmokeKernel kernel;
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    EXPECT_NO_THROW((void)kernel.deviceAvailable());
#else
    EXPECT_FALSE(kernel.deviceAvailable());
#endif
  }

  TEST(VulkanWavefrontSmokeKernel, RunsDummyHitMissKernelWhenEnabled) {
    VulkanWavefrontSmokeKernel kernel;
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }

    const std::vector<std::uint32_t> rayIds{0u, 1u, 0x12345678u, 0xffffffffu};
    const std::vector<std::uint32_t> results = kernel.runDummyHitMissKernel(rayIds);

    ASSERT_EQ(rayIds.size(), results.size());
    for (std::size_t i = 0; i < rayIds.size(); ++i) {
      EXPECT_EQ(rayIds[i] ^ 0xa5a5a5a5u, results[i]);
    }
#else
    EXPECT_THROW((void)kernel.runDummyHitMissKernel({1u}), std::runtime_error);
#endif
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresPlatformGpuAvailability) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(false, false, supportedPackedDiagnostics(), context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("unavailable"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresRenderPathAvailability) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, false, supportedPackedDiagnostics(), context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("render path"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresSupportedPackedScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;
    WavefrontIntersectionSceneDiagnostics diagnostics = supportedPackedDiagnostics();
    diagnostics.unsupportedPrimitives = 1;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, diagnostics, context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("unsupported"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresLargeEnoughExpectedRayCount) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 63;
    context.minimumGpuRayCount = 64;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, supportedPackedDiagnostics(), context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("below GPU threshold"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresEnoughRaysToAmortizeSceneUpload) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 127;
    context.minimumGpuRayCount = 0;
    context.minimumGpuRaysPerSceneUploadKiB = 64;
    WavefrontIntersectionSceneDiagnostics diagnostics = supportedPackedDiagnostics();
    diagnostics.uploadBytes = 2048;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, diagnostics, context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("below GPU threshold 128"));
    EXPECT_NE(std::string::npos, decision.reason.find("scene upload 2048 bytes"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicySelectsGpuWhenSceneUploadIsAmortized) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 128;
    context.minimumGpuRayCount = 0;
    context.minimumGpuRaysPerSceneUploadKiB = 64;
    WavefrontIntersectionSceneDiagnostics diagnostics = supportedPackedDiagnostics();
    diagnostics.uploadBytes = 2048;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, diagnostics, context);

    EXPECT_TRUE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("auto selected GPU"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresBasicKernelEligibleScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;
    WavefrontIntersectionSceneDiagnostics diagnostics = supportedPackedDiagnostics();
    diagnostics.triangleClosestHitKernelEligible = false;
    diagnostics.basicHitKernelEligible = false;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, diagnostics, context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("basic-kernel eligible"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicySelectsGpuForAvailableLargeSupportedScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 64;
    context.minimumGpuRayCount = 64;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, supportedPackedDiagnostics(), context);

    EXPECT_TRUE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("auto selected GPU"));
  }

  TEST(WavefrontIntersectionBackend, ChoiceParsesUserFacingBackendNames) {
    EXPECT_EQ(WavefrontIntersectionBackendChoice::automatic(),
              WavefrontIntersectionBackendChoice::fromString("auto"));
    EXPECT_EQ(WavefrontIntersectionBackendChoice::automatic(),
              WavefrontIntersectionBackendChoice::fromString("automatic"));
    EXPECT_EQ(WavefrontIntersectionBackendChoice::cpu(),
              WavefrontIntersectionBackendChoice::fromString("cpu"));
    EXPECT_EQ(WavefrontIntersectionBackendChoice::gpu(),
              WavefrontIntersectionBackendChoice::fromString("gpu"));
    EXPECT_EQ(WavefrontIntersectionBackendChoice::gpu(),
              WavefrontIntersectionBackendChoice::fromString("G-P_U"));
    EXPECT_THROW(WavefrontIntersectionBackendChoice::fromString("metal"), std::invalid_argument);
  }

  void expectGpuHitRecordNear(const GpuIntersectionHitRecord& actual,
                              const GpuIntersectionHitRecord& expected) {
    EXPECT_EQ(expected.hit, actual.hit);
    EXPECT_EQ(expected.material, actual.material);
    EXPECT_EQ(expected.object, actual.object);
    EXPECT_EQ(expected.primitiveRecord, actual.primitiveRecord);
    EXPECT_EQ(expected.rayIndex, actual.rayIndex);
    if (expected.hit == 0) {
      EXPECT_FALSE(actual.hit);
      return;
    }

    EXPECT_NEAR(expected.distance, actual.distance, 1e-5f);
    for (std::size_t index = 0; index != 4; ++index) {
      EXPECT_NEAR(expected.point[index], actual.point[index], 1e-5f);
      EXPECT_NEAR(expected.normal[index], actual.normal[index], 1e-5f);
      EXPECT_NEAR(expected.uv[index], actual.uv[index], 1e-5f);
      EXPECT_NEAR(expected.barycentric[index], actual.barycentric[index], 1e-5f);
    }
  }

  TEST(MetalWavefrontSmokeKernel, RunsDummyHitMissKernelWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    const std::vector<std::uint32_t> rayIds{0u, 1u, 0x12345678u, 0xffffffffu};
    const std::vector<std::uint32_t> results = kernel.runDummyHitMissKernel(rayIds);

    ASSERT_EQ(rayIds.size(), results.size());
    for (std::size_t i = 0; i < rayIds.size(); ++i) {
      EXPECT_EQ(rayIds[i] ^ 0xa5a5a5a5u, results[i]);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontSmokeKernel, RunsTriangleClosestHitKernelWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    Scene scene;
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 6), Vector3d(1, -1, 6), Vector3d(0, 1, 6)));
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 2), Vector3d(1, -1, 2), Vector3d(0, 1, 2)));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.triangleClosestHitKernelEligible());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 7),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 8),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 9, 0.0,
                                           1.0),
    };
    const std::vector<GpuIntersectionHitRecord> expected =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);

    const std::vector<GpuIntersectionHitRecord> actual =
      kernel.runBasicClosestHitKernel(buffers, rays);

    ASSERT_EQ(expected.size(), actual.size());
    for (std::size_t index = 0; index != expected.size(); ++index) {
      expectGpuHitRecordNear(actual[index], expected[index]);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontSmokeKernel, RunsTriangleAnyHitKernelWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    Scene scene;
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 6), Vector3d(1, -1, 6), Vector3d(0, 1, 6)));
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 2), Vector3d(1, -1, 2), Vector3d(0, 1, 2)));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.triangleClosestHitKernelEligible());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 7, 0.0,
                                           3.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 8, 0.0,
                                           3.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 9, 0.0,
                                           1.0),
    };

    const std::vector<GpuIntersectionOcclusionRecord> actual =
      kernel.runBasicAnyHitKernel(buffers, rays);

    ASSERT_EQ(rays.size(), actual.size());
    for (std::size_t index = 0; index != rays.size(); ++index) {
      EXPECT_EQ(rays[index].rayIndex, actual[index].rayIndex);
      EXPECT_EQ(GpuIntersectionIntersector().intersectAny(buffers, rays[index]) ? 1u : 0u,
                actual[index].occluded);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontSmokeKernel, RunsSphereBasicHitKernelsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 11),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 12),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 13, 0.0,
                                           1.0),
    };
    const std::vector<GpuIntersectionHitRecord> expected =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);

    const std::vector<GpuIntersectionHitRecord> actualClosest =
      kernel.runBasicClosestHitKernel(buffers, rays);
    const std::vector<GpuIntersectionHitRecord> repeatedClosest =
      kernel.runBasicClosestHitKernel(buffers, rays);
    ASSERT_EQ(expected.size(), actualClosest.size());
    ASSERT_EQ(expected.size(), repeatedClosest.size());
    for (std::size_t index = 0; index != expected.size(); ++index) {
      expectGpuHitRecordNear(actualClosest[index], expected[index]);
      expectGpuHitRecordNear(repeatedClosest[index], expected[index]);
    }

    const std::vector<GpuIntersectionOcclusionRecord> actualAny =
      kernel.runBasicAnyHitKernel(buffers, rays);
    const std::vector<GpuIntersectionOcclusionRecord> repeatedAny =
      kernel.runBasicAnyHitKernel(buffers, rays);
    ASSERT_EQ(rays.size(), actualAny.size());
    ASSERT_EQ(rays.size(), repeatedAny.size());
    for (std::size_t index = 0; index != rays.size(); ++index) {
      EXPECT_EQ(rays[index].rayIndex, actualAny[index].rayIndex);
      EXPECT_EQ(rays[index].rayIndex, repeatedAny[index].rayIndex);
      EXPECT_EQ(GpuIntersectionIntersector().intersectAny(buffers, rays[index]) ? 1u : 0u,
                actualAny[index].occluded);
      EXPECT_EQ(GpuIntersectionIntersector().intersectAny(buffers, rays[index]) ? 1u : 0u,
                repeatedAny[index].occluded);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontPreparedScene, ReusesPreparedBuffersAcrossClosestAndAnyHitQueries) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 6), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.basicHitKernelEligible());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 31),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 32),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 33, 0.0,
                                           2.5),
    };
    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);

    const MetalWavefrontPreparedScene prepared(buffers);
    const MetalWavefrontClosestHitKernelResult closest =
      prepared.runTimedBasicClosestHitKernel(rays);
    const MetalWavefrontClosestHitKernelResult repeatedClosest =
      prepared.runTimedBasicClosestHitKernel(rays);

    ASSERT_EQ(expectedClosest.size(), closest.hits.size());
    ASSERT_EQ(expectedClosest.size(), repeatedClosest.hits.size());
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(closest.hits[index], expectedClosest[index]);
      expectGpuHitRecordNear(repeatedClosest.hits[index], expectedClosest[index]);
    }

    const MetalWavefrontAnyHitKernelResult anyHit = prepared.runTimedBasicAnyHitKernel(rays);
    const MetalWavefrontAnyHitKernelResult repeatedAnyHit =
      prepared.runTimedBasicAnyHitKernel(rays);
    ASSERT_EQ(rays.size(), anyHit.records.size());
    ASSERT_EQ(rays.size(), repeatedAnyHit.records.size());
    for (std::size_t index = 0; index != rays.size(); ++index) {
      const std::uint32_t expectedOccluded =
        GpuIntersectionIntersector().intersectAny(buffers, rays[index]) ? 1u : 0u;
      EXPECT_EQ(rays[index].rayIndex, anyHit.records[index].rayIndex);
      EXPECT_EQ(expectedOccluded, anyHit.records[index].occluded);
      EXPECT_EQ(rays[index].rayIndex, repeatedAnyHit.records[index].rayIndex);
      EXPECT_EQ(expectedOccluded, repeatedAnyHit.records[index].occluded);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontSmokeKernel, RunsExactPrimitiveBasicHitKernelsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    Scene scene;
    scene.add(std::make_shared<Plane>(Vector3d(0, 0, 1), -9.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 5), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    scene.add(std::make_shared<Disk>(Vector3d(3, 0, 4), Vector3d(0, 0, 1), 1.0));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_TRUE(buffers.packedClosestHitKernelEligible());
    ASSERT_TRUE(buffers.packedAnyHitKernelEligible());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 21),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(3, 0, 0, 1), Vector3d(0, 0, 1)), 22),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(6, 0, 0, 1), Vector3d(0, 0, 1)), 23),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(3, 0, 0, 1), Vector3d(0, 0, 1)), 24, 0.0,
                                           3.5),
    };
    const std::vector<GpuIntersectionHitRecord> expected =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);

    const std::vector<GpuIntersectionHitRecord> actualClosest =
      kernel.runBasicClosestHitKernel(buffers, rays);
    ASSERT_EQ(expected.size(), actualClosest.size());
    for (std::size_t index = 0; index != expected.size(); ++index) {
      expectGpuHitRecordNear(actualClosest[index], expected[index]);
    }

    const std::vector<GpuIntersectionOcclusionRecord> actualAny =
      kernel.runBasicAnyHitKernel(buffers, rays);
    ASSERT_EQ(rays.size(), actualAny.size());
    for (std::size_t index = 0; index != rays.size(); ++index) {
      EXPECT_EQ(rays[index].rayIndex, actualAny[index].rayIndex);
      EXPECT_EQ(GpuIntersectionIntersector().intersectAny(buffers, rays[index]) ? 1u : 0u,
                actualAny[index].occluded);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontSmokeKernel, RejectsNonBasicHitSceneWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    scene.add(std::make_shared<Torus>(2.0, 0.5));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 11),
    };

    EXPECT_FALSE(buffers.basicHitKernelEligible());
    EXPECT_FALSE(buffers.packedClosestHitKernelEligible());
    EXPECT_FALSE(buffers.packedAnyHitKernelEligible());
    EXPECT_THROW(MetalWavefrontSmokeKernel().runBasicClosestHitKernel(buffers, rays),
                 std::invalid_argument);
    EXPECT_THROW(MetalWavefrontSmokeKernel().runBasicAnyHitKernel(buffers, rays),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(WavefrontIntersectionBackend, AutoChoiceCompilesSupportedSceneOnlyWhenPlatformAvailable) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::automatic().createBackendForScene(scene, context);

    EXPECT_STREQ("auto", backend->requestedName());
    EXPECT_STREQ("cpu", backend->name());
    EXPECT_STREQ("available", backend->availability());
    if (!hostPlatformIntersectionDeviceAvailable() ||
        !hostPlatformIntersectionRenderPathAvailable()) {
      EXPECT_STREQ("runtime_scene", backend->executionPath());
      EXPECT_NE(std::string::npos,
                std::string(backend->fallbackReason()).find("auto selected CPU"));
      const std::string reason = backend->fallbackReason();
      const bool disabled = reason.find("not enabled") != std::string::npos;
      const bool enabledWithoutClosestHitKernel =
        reason.find("no render-path closest-hit kernel") != std::string::npos;
      const bool enabledWithoutBasicHitKernel =
        reason.find("no render-path basic hit kernel") != std::string::npos;
      const bool enabledWithoutDevice = reason.find("no Metal device") != std::string::npos;
      const bool enabledWithoutVulkanComputeDevice =
        reason.find("no Vulkan compute device") != std::string::npos;
      EXPECT_TRUE(disabled || enabledWithoutClosestHitKernel || enabledWithoutBasicHitKernel ||
                  enabledWithoutDevice || enabledWithoutVulkanComputeDevice)
        << reason;
      EXPECT_EQ(nullptr, backend->compiledScene());
      EXPECT_FALSE(backend->compiledSceneDiagnostics().compiled);
      return;
    }

    EXPECT_NE(nullptr, backend->compiledScene());
    EXPECT_TRUE(backend->compiledSceneDiagnostics().compiled);
    EXPECT_TRUE(backend->compiledSceneDiagnostics().basicHitKernelEligible);
    if (std::string(backend->platformName()) == "vulkan") {
      EXPECT_STREQ("fallback", backend->availability());
      EXPECT_STREQ("packed_cpu", backend->executionPath());
      EXPECT_NE(std::string::npos,
                std::string(backend->fallbackReason()).find("no render-path closest-hit kernel"));
    }
  }

  TEST(WavefrontIntersectionBackend, GpuChoiceUsesHostPlatformFallbackForSupportedScene) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

#if defined(__APPLE__)
    if (std::string(backend->name()) == "metal") {
      EXPECT_STREQ("gpu", backend->requestedName());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
      EXPECT_STREQ("metal", backend->executionPath());
      EXPECT_STREQ("metal", backend->closestHitExecutionPath());
      EXPECT_STREQ("metal", backend->anyHitExecutionPath());
    } else {
      expectUnavailablePlatformFallback(*backend, "Metal", "packed_cpu", "metal");
    }
#else
    expectUnavailablePlatformFallback(*backend, "Vulkan", "packed_cpu", "vulkan");
#endif

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_TRUE(backend->compiledScene()->fullySupported());
    EXPECT_EQ(1u, backend->compiledScene()->primitives().size());
    EXPECT_EQ(1u, backend->compiledScene()->spheres().size());
    EXPECT_TRUE(backend->compiledScene()->unsupportedPrimitives().empty());
    EXPECT_EQ(backend->compiledScene()->bvh().size(),
              backend->gpuIntersectionSceneBuffers()->bvh.size());
    EXPECT_EQ(backend->compiledScene()->primitives().size(),
              backend->gpuIntersectionSceneBuffers()->primitives.size());

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(1u, diagnostics.bvhNodes);
    EXPECT_EQ(1u, diagnostics.primitives);
    EXPECT_EQ(0u, diagnostics.triangles);
    EXPECT_EQ(1u, diagnostics.spheres);
    EXPECT_EQ(0u, diagnostics.unsupportedPrimitives);
    EXPECT_EQ(backend->gpuIntersectionSceneBuffers()->uploadByteCount(), diagnostics.uploadBytes);
    EXPECT_FALSE(diagnostics.triangleClosestHitKernelEligible);
    EXPECT_TRUE(diagnostics.basicHitKernelEligible);
    EXPECT_TRUE(diagnostics.packedClosestHitKernelEligible);
    EXPECT_TRUE(diagnostics.packedAnyHitKernelEligible);
    EXPECT_GT(backend->estimatedClosestHitRayUploadBytes(4), 0u);
    EXPECT_GT(backend->estimatedClosestHitReadbackBytes(4), 0u);
    EXPECT_GT(backend->estimatedAnyHitRayUploadBytes(4), 0u);
    EXPECT_GT(backend->estimatedAnyHitReadbackBytes(4), 0u);
  }

  TEST(WavefrontIntersectionBackend, GpuChoiceDoesNotRetainUnsupportedCompiledScene) {
    auto torus = std::make_shared<Torus>(2.0, 0.5);
    torus->setName("exact torus");
    Scene scene;
    scene.add(torus);

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    EXPECT_STREQ("gpu", backend->requestedName());
    EXPECT_STREQ("cpu", backend->name());
    EXPECT_STREQ("fallback", backend->availability());
    EXPECT_STREQ("runtime_scene", backend->executionPath());
    EXPECT_EQ(nullptr, backend->compiledScene());
    EXPECT_EQ(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_NE(std::string::npos, std::string(backend->fallbackReason()).find("unsupported"));
    EXPECT_NE(std::string::npos, std::string(backend->fallbackReason()).find("exact torus"));

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(1u, diagnostics.primitives);
    EXPECT_EQ(1u, diagnostics.unsupportedPrimitives);
    EXPECT_GT(diagnostics.uploadBytes, 0u);
    EXPECT_FALSE(diagnostics.triangleClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.basicHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedAnyHitKernelEligible);
    EXPECT_EQ(0u, backend->estimatedClosestHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend->estimatedClosestHitReadbackBytes(4));
    EXPECT_EQ(0u, backend->estimatedAnyHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend->estimatedAnyHitReadbackBytes(4));
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuFallbackClosestHitUsesRetainedPackedSphereScene) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_FALSE(backend->gpuIntersectionSceneBuffers()->triangleClosestHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->basicHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedAnyHitKernelEligible());
    const bool usesMetalClosestHit = std::string(backend->closestHitExecutionPath()) == "metal";
    if (usesMetalClosestHit) {
      EXPECT_STREQ("metal", backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
      EXPECT_STREQ("metal", backend->executionPath());
      EXPECT_STREQ("metal", backend->anyHitExecutionPath());
    } else {
      EXPECT_STREQ("packed_cpu", backend->executionPath());
      EXPECT_STREQ("packed_cpu", backend->closestHitExecutionPath());
      EXPECT_STREQ("packed_cpu", backend->anyHitExecutionPath());
    }

    State state;
    HitPointInterval hitPoints;
    const Primitive* hit = backend->intersectClosest(
      emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), hitPoints, state);

    ASSERT_EQ(sphere.get(), hit);
    ASSERT_FALSE(hitPoints.minWithPositiveDistance().isUndefined());
    EXPECT_NEAR(2.0, hitPoints.minWithPositiveDistance().distance(), 1e-9);
    EXPECT_EQ(1, state.intersectionHits);
    EXPECT_EQ(0, state.intersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuClosestHitBatchUsesRetainedPackedSphereScene) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_FALSE(backend->prefersClosestHitBatch(1));
    EXPECT_TRUE(backend->prefersClosestHitBatch(2));
    EXPECT_TRUE(backend->prefersAnyHitBatch(1));
    EXPECT_TRUE(backend->prefersAnyHitBatch(2));

    State hitState;
    State missState;
    const std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &hitState},
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 3, -4), Vector3d(0, 0, 1)), &missState}};

    const std::vector<WavefrontClosestHitResult> hits =
      backend->intersectClosestBatch(emptyScene, queries);

    ASSERT_EQ(2u, hits.size());
    EXPECT_TRUE(hits[0].hit());
    EXPECT_EQ(sphere.get(), hits[0].primitive);
    EXPECT_NEAR(3.0, hits[0].hitPoint.distance(), 1e-5);
    EXPECT_FALSE(hits[1].hit());
    EXPECT_EQ(1, hitState.intersectionHits);
    EXPECT_EQ(0, hitState.intersectionMisses);
    EXPECT_EQ(0, missState.intersectionHits);
    EXPECT_EQ(1, missState.intersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedPackedQueriesReportPackedCpuTimingPath) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);
    Scene emptyScene;

    auto compiled = std::make_shared<const CompiledIntersectionScene>(
      IntersectionSceneCompiler().compile(sourceScene));
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      VulkanWavefrontIntersectionBackend::createPrepared(compiled);

    State closestState;
    const std::vector<WavefrontClosestHitQuery> closestQueries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &closestState}};
    WavefrontIntersectionQueryTiming closestTiming;
    const std::vector<WavefrontClosestHitResult> closestHits =
      backend->intersectClosestBatch(emptyScene, closestQueries, &closestTiming);

    ASSERT_EQ(1u, closestHits.size());
    EXPECT_TRUE(closestHits.front().hit());
    EXPECT_EQ("packed_cpu", closestTiming.executionPath);

    State anyState;
    const std::vector<WavefrontAnyHitQuery> anyQueries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 4.0, &anyState}};
    WavefrontIntersectionQueryTiming anyTiming;
    const std::vector<bool> anyHits =
      backend->intersectAnyBatch(emptyScene, anyQueries, &anyTiming);

    ASSERT_EQ(1u, anyHits.size());
    EXPECT_TRUE(anyHits.front());
    EXPECT_EQ("packed_cpu", anyTiming.executionPath);
  }

  TEST(WavefrontIntersectionBackend,
       PreparedGpuFallbackClosestHitUsesRetainedPackedStaticTransformScene) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    auto instance = std::make_shared<Instance>(triangle);
    instance->setMatrix(Matrix4d::translate(0, 0, 1));
    Scene sourceScene;
    sourceScene.add(instance);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_FALSE(backend->gpuIntersectionSceneBuffers()->triangleClosestHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->basicHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedAnyHitKernelEligible());
    if (std::string(backend->closestHitExecutionPath()) == "metal") {
      EXPECT_STREQ("metal", backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
      EXPECT_STREQ("metal", backend->executionPath());
      EXPECT_STREQ("metal", backend->anyHitExecutionPath());
    } else {
      EXPECT_STREQ("packed_cpu", backend->executionPath());
      EXPECT_STREQ("packed_cpu", backend->closestHitExecutionPath());
      EXPECT_STREQ("packed_cpu", backend->anyHitExecutionPath());
    }

    State state;
    HitPointInterval hitPoints;
    const Primitive* hit = backend->intersectClosest(
      emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), hitPoints, state);

    ASSERT_EQ(triangle.get(), hit);
    ASSERT_FALSE(hitPoints.minWithPositiveDistance().isUndefined());
    EXPECT_NEAR(4.0, hitPoints.minWithPositiveDistance().distance(), 1e-9);
    EXPECT_EQ(1, state.intersectionHits);
    EXPECT_EQ(0, state.intersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuFallbackClosestHitUsesRetainedPackedTriangleScene) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    Scene sourceScene;
    sourceScene.add(triangle);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->triangleClosestHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedAnyHitKernelEligible());
    const bool usesMetalClosestHit = std::string(backend->closestHitExecutionPath()) == "metal";
    if (usesMetalClosestHit) {
      EXPECT_STREQ("metal", backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
      EXPECT_STREQ("metal", backend->executionPath());
      EXPECT_STREQ("metal", backend->anyHitExecutionPath());
    } else {
      EXPECT_STREQ("packed_cpu", backend->executionPath());
      EXPECT_STREQ("packed_cpu", backend->closestHitExecutionPath());
      EXPECT_STREQ("packed_cpu", backend->anyHitExecutionPath());
    }

    State state;
    HitPointInterval hitPoints;
    const Primitive* hit = backend->intersectClosest(
      emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), hitPoints, state);

    ASSERT_EQ(triangle.get(), hit);
    ASSERT_FALSE(hitPoints.minWithPositiveDistance().isUndefined());
    EXPECT_NEAR(3.0, hitPoints.minWithPositiveDistance().distance(), 1e-6);
    EXPECT_EQ(Vector4d(0, 0, 0, 1), hitPoints.minWithPositiveDistance().point());
    EXPECT_EQ(Vector3d(0, 0, 1), hitPoints.minWithPositiveDistance().normal());
    EXPECT_EQ(1, state.intersectionHits);
    EXPECT_EQ(0, state.intersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuAnyHitUsesRetainedPackedTriangleScene) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    Scene sourceScene;
    sourceScene.add(triangle);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    ASSERT_TRUE(backend->gpuIntersectionSceneBuffers()->triangleClosestHitKernelEligible());
    ASSERT_TRUE(backend->gpuIntersectionSceneBuffers()->packedAnyHitKernelEligible());
    if (std::string(backend->anyHitExecutionPath()) == "metal") {
      EXPECT_STREQ("metal", backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
    } else {
      EXPECT_STREQ("packed_cpu", backend->anyHitExecutionPath());
    }

    State hitState;
    EXPECT_TRUE(backend->intersectAny(emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)),
                                      4.0, hitState));
    EXPECT_EQ(1, hitState.shadowIntersectionHits);
    EXPECT_EQ(0, hitState.shadowIntersectionMisses);

    State boundedMissState;
    EXPECT_FALSE(backend->intersectAny(emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)),
                                       2.0, boundedMissState));
    EXPECT_EQ(0, boundedMissState.shadowIntersectionHits);
    EXPECT_EQ(1, boundedMissState.shadowIntersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuFallbackAnyHitUsesRetainedPackedScene) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedAnyHitKernelEligible());
    if (std::string(backend->anyHitExecutionPath()) == "metal") {
      EXPECT_STREQ("metal", backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
    } else {
      EXPECT_STREQ("packed_cpu", backend->anyHitExecutionPath());
    }

    State hitState;
    EXPECT_TRUE(backend->intersectAny(emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)),
                                      3.0, hitState));
    EXPECT_EQ(1, hitState.shadowIntersectionHits);
    EXPECT_EQ(0, hitState.shadowIntersectionMisses);

    State missState;
    EXPECT_FALSE(backend->intersectAny(emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)),
                                       1.0, missState));
    EXPECT_EQ(0, missState.shadowIntersectionHits);
    EXPECT_EQ(1, missState.shadowIntersectionMisses);

    State batchHitState;
    State batchMissState;
    const std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 3.0, &batchHitState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &batchMissState}};
    WavefrontIntersectionQueryTiming timing;
    const std::vector<bool> occluded = backend->intersectAnyBatch(emptyScene, queries, &timing);

    ASSERT_EQ(2u, occluded.size());
    EXPECT_TRUE(occluded[0]);
    EXPECT_FALSE(occluded[1]);
    EXPECT_EQ(1, batchHitState.shadowIntersectionHits);
    EXPECT_EQ(0, batchHitState.shadowIntersectionMisses);
    EXPECT_EQ(0, batchMissState.shadowIntersectionHits);
    EXPECT_EQ(1, batchMissState.shadowIntersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuFallbackPacketHitUsesRetainedPackedSphereScene) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    std::array<Rayd, Ray4::lanes> rays{
      Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)),
      Rayd(Vector4d(3, 0, -3, 1), Vector3d(0, 0, 1)),
      Rayd::undefined,
      Rayd::undefined,
    };
    State hitState;
    State missState;
    PrimitivePacketState4 states{};
    states[0] = &hitState;
    states[1] = &missState;

    const PrimitivePacketHit4 packet =
      backend->intersectPacketClosest(emptyScene, Ray4(rays), states);

    ASSERT_TRUE(packet.hit(0));
    EXPECT_EQ(sphere.get(), packet.primitive(0));
    EXPECT_NEAR(2.0, packet.hitPoint(0).distance(), 1e-9);
    EXPECT_FALSE(packet.hit(1));
    EXPECT_EQ(1, hitState.intersectionHits);
    EXPECT_EQ(0, hitState.intersectionMisses);
    EXPECT_EQ(0, missState.intersectionHits);
    EXPECT_EQ(1, missState.intersectionMisses);
  }

  TEST(WavefrontIntersectionBackend,
       PreparedGpuFallbackRay8PacketHitUsesRetainedPackedTriangleScene) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    Scene sourceScene;
    sourceScene.add(triangle);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    ASSERT_TRUE(backend->gpuIntersectionSceneBuffers()->triangleClosestHitKernelEligible());

    std::array<Rayd, Ray8::lanes> rays{
      Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)),
      Rayd(Vector4d(3, 0, -3, 1), Vector3d(0, 0, 1)),
      Rayd::undefined,
      Rayd::undefined,
      Rayd::undefined,
      Rayd::undefined,
      Rayd::undefined,
      Rayd::undefined,
    };
    State hitState;
    State missState;
    PrimitivePacketState8 states{};
    states[0] = &hitState;
    states[1] = &missState;

    const PrimitivePacketHit8 packet =
      backend->intersectPacketClosest(emptyScene, Ray8(rays), states);

    ASSERT_TRUE(packet.hit(0));
    EXPECT_EQ(triangle.get(), packet.primitive(0));
    EXPECT_NEAR(3.0, packet.hitPoint(0).distance(), 1e-6);
    EXPECT_EQ(Vector4d(0, 0, 0, 1), packet.hitPoint(0).point());
    EXPECT_EQ(Vector3d(0, 0, 1), packet.hitPoint(0).normal());
    EXPECT_FALSE(packet.hit(1));
    EXPECT_EQ(1, hitState.intersectionHits);
    EXPECT_EQ(0, hitState.intersectionMisses);
    EXPECT_EQ(0, missState.intersectionHits);
    EXPECT_EQ(1, missState.intersectionMisses);
  }
}
