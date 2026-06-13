#include <gtest/gtest.h>

#include "render/IntersectionService.h"
#include "render/State.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"

#include "core/geometry/Polyline.h"
#include "core/math/HitPoint.h"

#include <memory>
#include <string>
#include <vector>

namespace IntersectionServiceTest {
  using namespace render;

  namespace {
    Scene sphereScene() {
      Scene scene;
      scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
      return scene;
    }
  }

  TEST(IntersectionService, SubmitsClosestHitWorkThroughPreparedBackend) {
    Scene scene = sphereScene();
    IntersectionService service(scene, WavefrontIntersectionBackendChoice::cpu());

    State state;
    const WavefrontClosestHitResult hit =
      service.closestHit(Rayd(Vector3d(0, 0, 0), Vector3d::forward()), state);

    ASSERT_TRUE(hit.hit());
    EXPECT_NE(nullptr, hit.primitive);
    EXPECT_NEAR(2.0, hit.hitPoint.distance(), 1e-9);
    EXPECT_EQ("cpu", service.diagnostics().requestedBackend);
    EXPECT_EQ("cpu", service.diagnostics().selectedBackend);
    EXPECT_EQ("available", service.diagnostics().availability);
    EXPECT_EQ("runtime_scene", service.diagnostics().executionPath);
    EXPECT_EQ("runtime_scene", service.diagnostics().closestHitExecutionPath);
    EXPECT_TRUE(service.diagnostics().fallbackReason.empty());
  }

  TEST(IntersectionService, SubmitsClosestHitBatchInSubmissionOrder) {
    Scene scene = sphereScene();
    IntersectionService service(scene, WavefrontIntersectionBackendChoice::cpu());
    State firstState;
    State secondState;
    const std::vector<WavefrontClosestHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), &firstState},
      {Rayd(Vector3d(4, 0, 0), Vector3d::forward()), &secondState},
    };

    const std::vector<WavefrontClosestHitResult> hits = service.closestHits(queries);

    ASSERT_EQ(2u, hits.size());
    EXPECT_TRUE(hits[0].hit());
    EXPECT_FALSE(hits[1].hit());
    EXPECT_EQ("runtime_scene", service.diagnostics().lastClosestHitTiming.executionPath);
  }

  TEST(IntersectionService, SubmitsAnyHitWorkThroughPreparedBackend) {
    Scene scene = sphereScene();
    IntersectionService service(scene, WavefrontIntersectionBackendChoice::cpu());

    State occludedState;
    EXPECT_TRUE(service.anyHit(Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, occludedState));

    State visibleState;
    EXPECT_FALSE(service.anyHit(Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, visibleState));
    EXPECT_EQ("runtime_scene", service.diagnostics().executionPath);
    EXPECT_EQ("runtime_scene", service.diagnostics().anyHitExecutionPath);
    EXPECT_TRUE(service.diagnostics().fallbackReason.empty());
  }

  TEST(IntersectionService, SubmitsAnyHitBatchInSubmissionOrder) {
    Scene scene = sphereScene();
    IntersectionService service(scene, WavefrontIntersectionBackendChoice::cpu());
    State firstState;
    State secondState;
    const std::vector<WavefrontAnyHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, &firstState},
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, &secondState},
    };

    const WavefrontOcclusionFlags occluded = service.anyHits(queries);

    ASSERT_EQ(2u, occluded.size());
    EXPECT_NE(0, occluded[0]);
    EXPECT_EQ(0, occluded[1]);
    EXPECT_EQ("runtime_scene", service.diagnostics().lastAnyHitTiming.executionPath);
  }

  TEST(IntersectionService, ReportsFallbackForUnsupportedGpuScene) {
    Scene scene;
    auto curve =
      std::make_shared<Curve>(core::Polyline({Vector3d(0, 0, 0), Vector3d(1, 0, 0)}), 0.1);
    curve->setName("debug curve");
    scene.add(curve);

    IntersectionService service(scene, WavefrontIntersectionBackendChoice::gpu());

    EXPECT_EQ("gpu", service.diagnostics().requestedBackend);
    EXPECT_EQ("cpu", service.diagnostics().selectedBackend);
    EXPECT_EQ("fallback", service.diagnostics().availability);
    EXPECT_EQ("runtime_scene", service.diagnostics().executionPath);
    EXPECT_NE(std::string::npos, service.diagnostics().fallbackReason.find("unsupported"));
    EXPECT_NE(std::string::npos, service.diagnostics().fallbackReason.find("debug curve"));
    EXPECT_TRUE(service.diagnostics().scene.compiled);
    EXPECT_EQ(1u, service.diagnostics().scene.unsupportedPrimitives);
  }
}
