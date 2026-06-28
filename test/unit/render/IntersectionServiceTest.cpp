#include <gtest/gtest.h>

#include "render/IntersectionService.h"
#include "render/GpuIntersectionScene.h"
#include "render/State.h"
#include "render/primitives/Difference.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"

#include "core/math/HitPoint.h"

#include <memory>
#include <stdexcept>
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

    class MixedQueryPathBackend final : public WavefrontIntersectionBackend {
    public:
      const char* name() const override {
        return "test";
      }

      const char* executionPath() const override {
        return "platform_nominal";
      }

      const Primitive* intersectClosest(const Scene& scene, const Rayd& ray,
                                        HitPointInterval& hitPoints, State& state,
                                        WavefrontIntersectionQueryTiming* timing) const override {
        if (timing) {
          timing->recordExecutionPath("packed_cpu");
          timing->recordFallbackReason("closest query fell back");
        }
        return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                            state);
      }

      bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                        WavefrontIntersectionQueryTiming* timing) const override {
        if (timing) {
          timing->recordExecutionPath("metal");
          timing->recordFallbackReason("any-hit query used platform path");
        }
        return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance,
                                                                        state);
      }

      PrimitivePacketHit4 intersectPacketClosest(const Scene& scene, const Ray4& rays,
                                                 const PrimitivePacketState4& states,
                                                 WavefrontIntersectionQueryTiming*) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states);
      }

      PrimitivePacketHit8 intersectPacketClosest(const Scene& scene, const Ray8& rays,
                                                 const PrimitivePacketState8& states,
                                                 WavefrontIntersectionQueryTiming*) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states);
      }
    };

    class ShortResultBackend final : public WavefrontIntersectionBackend {
    public:
      const char* name() const override {
        return "short_result";
      }

      const Primitive* intersectClosest(const Scene& scene, const Rayd& ray,
                                        HitPointInterval& hitPoints, State& state,
                                        WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                            state, timing);
      }

      std::vector<WavefrontClosestHitResult>
      intersectClosestBatch(const Scene&, const std::vector<WavefrontClosestHitQuery>& queries,
                            WavefrontIntersectionQueryTiming*) const override {
        if (queries.empty()) {
          return {};
        }
        return std::vector<WavefrontClosestHitResult>(queries.size() - 1);
      }

      bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                        WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance,
                                                                        state, timing);
      }

      WavefrontOcclusionFlags intersectAnyBatch(const Scene&,
                                                const std::vector<WavefrontAnyHitQuery>& queries,
                                                WavefrontIntersectionQueryTiming*) const override {
        if (queries.empty()) {
          return {};
        }
        return WavefrontOcclusionFlags(queries.size() - 1);
      }

      WavefrontOcclusionFlags
      intersectAnyFrontier(const Scene&, const WavefrontAnyHitFrontier& frontier,
                           WavefrontIntersectionQueryTiming*) const override {
        if (frontier.rayCount() == 0) {
          return {};
        }
        return WavefrontOcclusionFlags(static_cast<std::size_t>(frontier.rayCount() - 1));
      }

      PrimitivePacketHit4
      intersectPacketClosest(const Scene& scene, const Ray4& rays,
                             const PrimitivePacketState4& states,
                             WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }

      PrimitivePacketHit8
      intersectPacketClosest(const Scene& scene, const Ray8& rays,
                             const PrimitivePacketState8& states,
                             WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }
    };

    class NullFrontierBackend final : public WavefrontIntersectionBackend {
    public:
      const char* name() const override {
        return "null_frontier";
      }

      std::unique_ptr<WavefrontClosestHitFrontier>
      createClosestHitFrontier(std::vector<WavefrontClosestHitQuery>) const override {
        return nullptr;
      }

      std::unique_ptr<WavefrontAnyHitFrontier>
      createAnyHitFrontier(std::vector<WavefrontAnyHitQuery>) const override {
        return nullptr;
      }

      const Primitive* intersectClosest(const Scene& scene, const Rayd& ray,
                                        HitPointInterval& hitPoints, State& state,
                                        WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                            state, timing);
      }

      bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                        WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance,
                                                                        state, timing);
      }

      PrimitivePacketHit4
      intersectPacketClosest(const Scene& scene, const Ray4& rays,
                             const PrimitivePacketState4& states,
                             WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }

      PrimitivePacketHit8
      intersectPacketClosest(const Scene& scene, const Ray8& rays,
                             const PrimitivePacketState8& states,
                             WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }
    };

    class DirectLightVisibilityCountingBackend : public WavefrontIntersectionBackend {
    public:
      const char* name() const override {
        return "direct_light_counting";
      }

      WavefrontDirectLightVisibilityBatchResult
      resolveDirectLightVisibilityBatch(const Scene& scene,
                                        std::vector<WavefrontAnyHitQuery> queries) const override {
        ++directLightVisibilityBatches;
        directLightVisibilityBatchSizes.push_back(queries.size());
        return WavefrontIntersectionBackend::resolveDirectLightVisibilityBatch(scene,
                                                                               std::move(queries));
      }

      const Primitive* intersectClosest(const Scene& scene, const Rayd& ray,
                                        HitPointInterval& hitPoints, State& state,
                                        WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                            state, timing);
      }

      bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                        WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance,
                                                                        state, timing);
      }

      PrimitivePacketHit4
      intersectPacketClosest(const Scene& scene, const Ray4& rays,
                             const PrimitivePacketState4& states,
                             WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }

      PrimitivePacketHit8
      intersectPacketClosest(const Scene& scene, const Ray8& rays,
                             const PrimitivePacketState8& states,
                             WavefrontIntersectionQueryTiming* timing) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }

      mutable int directLightVisibilityBatches{0};
      mutable std::vector<std::size_t> directLightVisibilityBatchSizes;
    };

    class ShortDirectLightVisibilityFrontierBackend final
        : public DirectLightVisibilityCountingBackend {
    public:
      const char* name() const override {
        return "short_direct_light_frontier";
      }

      WavefrontDirectLightVisibilityBatchResult
      resolveDirectLightVisibilityBatch(const Scene&,
                                        std::vector<WavefrontAnyHitQuery> queries) const override {
        ++directLightVisibilityBatches;
        directLightVisibilityBatchSizes.push_back(queries.size());
        if (!queries.empty()) {
          queries.pop_back();
        }
        WavefrontDirectLightVisibilityBatchResult result;
        result.frontier = createAnyHitFrontier(std::move(queries));
        result.occluded = WavefrontOcclusionFlags(
          result.frontier ? static_cast<std::size_t>(result.frontier->rayCount()) : 0U, 0U);
        return result;
      }
    };
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

  TEST(IntersectionService, SubmitsClosestHitFrontierInSubmissionOrder) {
    Scene scene = sphereScene();
    IntersectionService service(scene, WavefrontIntersectionBackendChoice::cpu());
    State firstState;
    State secondState;
    std::vector<WavefrontClosestHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), &firstState},
      {Rayd(Vector3d(4, 0, 0), Vector3d::forward()), &secondState},
    };
    std::unique_ptr<WavefrontClosestHitFrontier> frontier =
      service.backend().createClosestHitFrontier(std::move(queries));

    const std::vector<WavefrontClosestHitResult> hits = service.closestHits(*frontier);

    ASSERT_EQ(2u, hits.size());
    EXPECT_TRUE(hits[0].hit());
    EXPECT_FALSE(hits[1].hit());
    EXPECT_EQ(2u, service.diagnostics().closestHitQueryCount);
    EXPECT_EQ(1u, service.diagnostics().closestHitHitCount);
    EXPECT_EQ("host", service.diagnostics().closestHitFrontierResidency);
    EXPECT_EQ(0u, service.diagnostics().closestHitFrontierPackedRayBytes);
    EXPECT_EQ(0u, service.diagnostics().closestHitFrontierHostPackedRayBytes);
    EXPECT_EQ(2u * sizeof(WavefrontClosestHitQuery),
              service.diagnostics().closestHitFrontierHostQueryBytes);
    EXPECT_EQ(0u, service.diagnostics().closestHitFrontierStateHandleBytes);
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

  TEST(IntersectionService, SubmitsDirectLightVisibilityThroughBackendHook) {
    Scene scene = sphereScene();
    DirectLightVisibilityCountingBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    const std::vector<WavefrontAnyHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, &firstState},
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, &secondState},
    };

    const WavefrontOcclusionFlags occluded = service.resolveDirectLightVisibility(queries);

    ASSERT_EQ(2u, occluded.size());
    EXPECT_NE(0, occluded[0]);
    EXPECT_EQ(0, occluded[1]);
    EXPECT_EQ(1, backend.directLightVisibilityBatches);
    EXPECT_EQ((std::vector<std::size_t>{2u}), backend.directLightVisibilityBatchSizes);
    EXPECT_EQ(2u, service.diagnostics().anyHitQueryCount);
    EXPECT_EQ(1u, service.diagnostics().anyHitOccludedCount);
    EXPECT_EQ("host", service.diagnostics().anyHitFrontierResidency);
    EXPECT_EQ(0u, service.diagnostics().anyHitFrontierPackedRayBytes);
    EXPECT_EQ(0u, service.diagnostics().anyHitFrontierHostPackedRayBytes);
    EXPECT_EQ(2u * sizeof(WavefrontAnyHitQuery),
              service.diagnostics().anyHitFrontierHostQueryBytes);
    EXPECT_EQ(0u, service.diagnostics().anyHitFrontierStateHandleBytes);
    EXPECT_EQ("runtime_scene", service.diagnostics().lastAnyHitTiming.executionPath);
  }

  TEST(IntersectionService, AccumulatesQueryCountsAndTransferEstimates) {
    Scene scene = sphereScene();
    IntersectionService service(scene, WavefrontIntersectionBackendChoice::gpu());
    State closestState;
    State missState;
    const std::vector<WavefrontClosestHitQuery> closestQueries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), &closestState},
      {Rayd(Vector3d(4, 0, 0), Vector3d::forward()), &missState},
    };

    const std::vector<WavefrontClosestHitResult> hits = service.closestHits(closestQueries);

    State occludedState;
    State visibleState;
    const std::vector<WavefrontAnyHitQuery> anyHitQueries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, &occludedState},
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, &visibleState},
    };

    const WavefrontOcclusionFlags occluded = service.anyHits(anyHitQueries);

    ASSERT_EQ(2u, hits.size());
    ASSERT_EQ(2u, occluded.size());
    const IntersectionServiceDiagnostics& diagnostics = service.diagnostics();
    EXPECT_EQ(2u, diagnostics.closestHitQueryCount);
    EXPECT_EQ(1u, diagnostics.closestHitHitCount);
    EXPECT_EQ(2u, diagnostics.anyHitQueryCount);
    EXPECT_EQ(1u, diagnostics.anyHitOccludedCount);
    EXPECT_EQ(service.backend().estimatedClosestHitRayUploadBytes(2),
              diagnostics.closestHitRayUploadBytesEstimate);
    EXPECT_EQ(service.backend().estimatedClosestHitReadbackBytes(2),
              diagnostics.closestHitReadbackBytesEstimate);
    EXPECT_EQ(service.backend().estimatedAnyHitRayUploadBytes(2),
              diagnostics.anyHitRayUploadBytesEstimate);
    EXPECT_EQ(service.backend().estimatedAnyHitReadbackBytes(2),
              diagnostics.anyHitReadbackBytesEstimate);
    EXPECT_EQ(diagnostics.closestHitRayUploadBytesEstimate +
                diagnostics.closestHitReadbackBytesEstimate +
                diagnostics.anyHitRayUploadBytesEstimate + diagnostics.anyHitReadbackBytesEstimate,
              diagnostics.queryTransferBytesEstimate);
    EXPECT_EQ("packed_host", diagnostics.closestHitFrontierResidency);
    EXPECT_EQ("packed_host", diagnostics.anyHitFrontierResidency);
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), diagnostics.closestHitFrontierPackedRayBytes);
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), diagnostics.closestHitFrontierHostPackedRayBytes);
    EXPECT_EQ(0u, diagnostics.closestHitFrontierHostQueryBytes);
    EXPECT_EQ(2u * sizeof(State*), diagnostics.closestHitFrontierStateHandleBytes);
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), diagnostics.anyHitFrontierPackedRayBytes);
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), diagnostics.anyHitFrontierHostPackedRayBytes);
    EXPECT_EQ(0u, diagnostics.anyHitFrontierHostQueryBytes);
    EXPECT_EQ(2u * sizeof(State*), diagnostics.anyHitFrontierStateHandleBytes);
  }

  TEST(IntersectionService, RejectsMismatchedClosestHitBatchResults) {
    Scene scene = sphereScene();
    const ShortResultBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    const std::vector<WavefrontClosestHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), &firstState},
      {Rayd(Vector3d(4, 0, 0), Vector3d::forward()), &secondState},
    };

    EXPECT_THROW(
      {
        const std::vector<WavefrontClosestHitResult> hits = service.closestHits(queries);
        (void)hits;
      },
      std::logic_error);
  }

  TEST(IntersectionService, RejectsMissingClosestHitFrontier) {
    Scene scene = sphereScene();
    const NullFrontierBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    const std::vector<WavefrontClosestHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), &firstState},
      {Rayd(Vector3d(4, 0, 0), Vector3d::forward()), &secondState},
    };

    EXPECT_THROW(
      {
        const std::vector<WavefrontClosestHitResult> hits = service.closestHits(queries);
        (void)hits;
      },
      std::logic_error);
  }

  TEST(IntersectionService, RejectsMismatchedClosestHitFrontierResults) {
    Scene scene = sphereScene();
    const ShortResultBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    std::vector<WavefrontClosestHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), &firstState},
      {Rayd(Vector3d(4, 0, 0), Vector3d::forward()), &secondState},
    };
    std::unique_ptr<WavefrontClosestHitFrontier> frontier =
      service.backend().createClosestHitFrontier(std::move(queries));

    EXPECT_THROW(
      {
        const std::vector<WavefrontClosestHitResult> hits = service.closestHits(*frontier);
        (void)hits;
      },
      std::logic_error);
  }

  TEST(IntersectionService, RejectsMismatchedAnyHitBatchResults) {
    Scene scene = sphereScene();
    const ShortResultBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    const std::vector<WavefrontAnyHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, &firstState},
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, &secondState},
    };

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = service.anyHits(queries);
        (void)occluded;
      },
      std::logic_error);
  }

  TEST(IntersectionService, RejectsMissingAnyHitFrontier) {
    Scene scene = sphereScene();
    const NullFrontierBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    const std::vector<WavefrontAnyHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, &firstState},
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, &secondState},
    };

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = service.anyHits(queries);
        (void)occluded;
      },
      std::logic_error);
  }

  TEST(IntersectionService, RejectsMismatchedAnyHitFrontierResults) {
    Scene scene = sphereScene();
    const ShortResultBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    std::vector<WavefrontAnyHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, &firstState},
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, &secondState},
    };
    std::unique_ptr<WavefrontAnyHitFrontier> frontier =
      service.backend().createAnyHitFrontier(std::move(queries));

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = service.anyHits(*frontier);
        (void)occluded;
      },
      std::logic_error);
  }

  TEST(IntersectionService, RejectsMismatchedDirectLightVisibilityResults) {
    Scene scene = sphereScene();
    const ShortResultBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    const std::vector<WavefrontAnyHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, &firstState},
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, &secondState},
    };

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = service.resolveDirectLightVisibility(queries);
        (void)occluded;
      },
      std::logic_error);
  }

  TEST(IntersectionService, RejectsMismatchedDirectLightVisibilityFrontier) {
    Scene scene = sphereScene();
    const ShortDirectLightVisibilityFrontierBackend backend;
    IntersectionService service(scene, backend);
    State firstState;
    State secondState;
    const std::vector<WavefrontAnyHitQuery> queries{
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, &firstState},
      {Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 1.0, &secondState},
    };

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = service.resolveDirectLightVisibility(queries);
        (void)occluded;
      },
      std::logic_error);
    EXPECT_EQ(1, backend.directLightVisibilityBatches);
    EXPECT_EQ((std::vector<std::size_t>{2u}), backend.directLightVisibilityBatchSizes);
  }

  TEST(IntersectionService, ReportsMixedObservedExecutionPathAcrossQueryFamilies) {
    Scene scene = sphereScene();
    const MixedQueryPathBackend backend;
    IntersectionService service(scene, backend);

    State closestState;
    const WavefrontClosestHitResult hit =
      service.closestHit(Rayd(Vector3d(0, 0, 0), Vector3d::forward()), closestState);
    ASSERT_TRUE(hit.hit());
    EXPECT_EQ("packed_cpu", service.diagnostics().executionPath);
    EXPECT_EQ("packed_cpu", service.diagnostics().closestHitExecutionPath);
    EXPECT_EQ("platform_nominal", service.diagnostics().anyHitExecutionPath);
    EXPECT_EQ("closest query fell back", service.diagnostics().fallbackReason);

    State anyState;
    EXPECT_TRUE(service.anyHit(Rayd(Vector3d(0, 0, 0), Vector3d::forward()), 4.0, anyState));

    EXPECT_EQ("mixed", service.diagnostics().executionPath);
    EXPECT_EQ("packed_cpu", service.diagnostics().closestHitExecutionPath);
    EXPECT_EQ("metal", service.diagnostics().anyHitExecutionPath);
    EXPECT_EQ("mixed", service.diagnostics().fallbackReason);
  }

  TEST(IntersectionService, ReportsFallbackForUnsupportedGpuScene) {
    Scene scene;
    auto difference = std::make_shared<Difference>();
    difference->setName("debug difference");
    difference->add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    difference->add(std::make_shared<Sphere>(Vector3d(0.5, 0, 3), 1.0));
    scene.add(difference);

    IntersectionService service(scene, WavefrontIntersectionBackendChoice::gpu());

    EXPECT_EQ("gpu", service.diagnostics().requestedBackend);
    EXPECT_EQ("cpu", service.diagnostics().selectedBackend);
    EXPECT_EQ("fallback", service.diagnostics().availability);
    EXPECT_EQ("runtime_scene", service.diagnostics().executionPath);
    EXPECT_NE(std::string::npos, service.diagnostics().fallbackReason.find("unsupported"));
    EXPECT_NE(std::string::npos, service.diagnostics().fallbackReason.find("debug difference"));
    EXPECT_TRUE(service.diagnostics().scene.compiled);
    EXPECT_EQ(1u, service.diagnostics().scene.unsupportedPrimitives);
  }
}
