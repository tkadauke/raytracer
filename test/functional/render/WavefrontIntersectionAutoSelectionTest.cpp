#include <gtest/gtest.h>

#include "core/geometry/Polyline.h"
#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/IntersectionService.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"

#include <memory>
#include <string>

namespace WavefrontIntersectionAutoSelectionTest {
  using namespace render;

  namespace {
    Scene supportedScene() {
      Scene scene;
      scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
      return scene;
    }

    Scene unsupportedScene() {
      Scene scene;
      auto curve =
        std::make_shared<Curve>(core::Polyline({Vector3d(0, 0, 0), Vector3d(1, 0, 0)}), 0.1);
      curve->setName("auto fallback curve");
      scene.add(curve);
      return scene;
    }

    WavefrontIntersectionBackendSelectionContext selectionContext(std::uint64_t expectedRays,
                                                                  std::uint64_t minimumGpuRays) {
      WavefrontIntersectionBackendSelectionContext context;
      context.expectedRayCount = expectedRays;
      context.minimumGpuRayCount = minimumGpuRays;
      return context;
    }

    WavefrontIntersectionSceneDiagnostics supportedDiagnostics() {
      Scene scene = supportedScene();
      const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
      const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
      return WavefrontIntersectionSceneDiagnostics::fromCompiledSceneAndUploadBuffers(compiled,
                                                                                      buffers);
    }

    WavefrontIntersectionSceneDiagnostics unsupportedDiagnostics() {
      Scene scene = unsupportedScene();
      const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
      return WavefrontIntersectionSceneDiagnostics::fromCompiledScene(compiled);
    }
  }

  TEST(WavefrontIntersectionAutoSelection, KeepsSmallSupportedSceneOnCpuBeforeCompiling) {
    Scene scene = supportedScene();
    IntersectionService service(scene, WavefrontIntersectionBackendChoice::automatic(),
                                selectionContext(63, 64));

    EXPECT_EQ("auto", service.diagnostics().requestedBackend);
    EXPECT_EQ("cpu", service.diagnostics().selectedBackend);
    EXPECT_EQ("available", service.diagnostics().availability);
    EXPECT_EQ("runtime_scene", service.diagnostics().executionPath);
    EXPECT_FALSE(service.diagnostics().scene.compiled);

    const std::string reason = service.diagnostics().fallbackReason;
    EXPECT_NE(std::string::npos, reason.find("auto selected CPU"));
    EXPECT_NE(std::string::npos, reason.find("expected ray count 63"));
    EXPECT_NE(std::string::npos, reason.find("fixed GPU threshold 64"));
    EXPECT_NE(std::string::npos, reason.find("before scene compilation"));
  }

  TEST(WavefrontIntersectionAutoSelection,
       ReportsLargeSupportedSceneAsGpuCandidateOrExplicitCpuFallback) {
    Scene scene = supportedScene();
    IntersectionService service(scene, WavefrontIntersectionBackendChoice::automatic(),
                                selectionContext(1000000, 64));

    EXPECT_EQ("auto", service.diagnostics().requestedBackend);

    if (!service.diagnostics().scene.compiled) {
      EXPECT_EQ("cpu", service.diagnostics().selectedBackend);
      EXPECT_EQ("available", service.diagnostics().availability);
      EXPECT_EQ("runtime_scene", service.diagnostics().executionPath);
      EXPECT_NE(std::string::npos,
                service.diagnostics().fallbackReason.find("platform GPU intersection backend"));
      return;
    }

    EXPECT_EQ(0u, service.diagnostics().scene.unsupportedPrimitives);
    EXPECT_TRUE(service.diagnostics().scene.basicHitKernelEligible);
    EXPECT_TRUE(service.diagnostics().scene.packedClosestHitKernelEligible);
    EXPECT_TRUE(service.diagnostics().scene.packedAnyHitKernelEligible);
    EXPECT_NE("runtime_scene", service.diagnostics().executionPath);
    EXPECT_TRUE(service.diagnostics().selectedBackend == "cpu" ||
                service.diagnostics().selectedBackend == "metal" ||
                service.diagnostics().selectedBackend == "vulkan");
    EXPECT_TRUE(service.diagnostics().availability == "available" ||
                service.diagnostics().availability == "fallback");
  }

  TEST(WavefrontIntersectionAutoSelection, PolicySelectsGpuForSupportedLargeCandidate) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    const WavefrontIntersectionBackendSelectionContext context = selectionContext(1000000, 64);

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, supportedDiagnostics(), context);

    EXPECT_TRUE(decision.useGpu);
    EXPECT_EQ(64u, decision.minimumExpectedRayCount);
    EXPECT_GT(decision.estimatedQueryTransferBytes, 0u);
    EXPECT_NE(std::string::npos, decision.reason.find("auto selected GPU"));
  }

  TEST(WavefrontIntersectionAutoSelection, PolicyFallsBackForUnsupportedScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    const WavefrontIntersectionBackendSelectionContext context = selectionContext(1000000, 64);

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, unsupportedDiagnostics(), context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_EQ(64u, decision.minimumExpectedRayCount);
    EXPECT_EQ(0u, decision.estimatedQueryTransferBytes);
    EXPECT_NE(std::string::npos,
              decision.reason.find("intersection scene contains unsupported primitives"));
  }
}
