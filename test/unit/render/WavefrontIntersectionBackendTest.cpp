#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "core/math/Matrix.h"
#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/State.h"
#include "render/VulkanWavefrontSmokeKernel.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/TransparentMaterial.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalWavefrontSmokeKernel.h"
#endif
#include "render/primitives/FlatMeshTriangle.h"
#include "render/primitives/ClosedSolidUnion.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Disk.h"
#include "render/primitives/Instance.h"
#include "render/primitives/OpenCylinder.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Triangle.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/geometry/Mesh.h"

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
        reason.find("no render-path basic hit kernel") != std::string::npos ||
        reason.find("no render-path triangle/sphere hit kernel") != std::string::npos ||
        reason.find("no render-path exact-primitive hit kernel") != std::string::npos;
      const bool enabledWithoutDevice = reason.find("no Metal device") != std::string::npos;
      const bool enabledWithoutVulkanComputeDevice =
        reason.find("no Vulkan compute device") != std::string::npos;
      const bool notTriangleEligible =
        reason.find("not eligible for the Metal triangle") != std::string::npos;
      const bool noPreparedTriangleScene =
        reason.find("no prepared triangle scene") != std::string::npos ||
        reason.find("no prepared triangle/sphere scene") != std::string::npos ||
        reason.find("no prepared exact-primitive scene") != std::string::npos;
      const bool notBasicEligible =
        reason.find("not eligible for the Metal basic") != std::string::npos ||
        reason.find("not eligible for the Vulkan triangle/sphere") != std::string::npos ||
        reason.find("not eligible for the Vulkan exact-primitive") != std::string::npos;
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

    std::unique_ptr<Mesh> triangleMeshAt(double xOffset, double z) {
      auto mesh = std::make_unique<Mesh>();
      mesh->addVertex(Vector3d(xOffset - 1, -1, z), Vector3d(0, 0, 1), Vector2d(0, 0));
      mesh->addVertex(Vector3d(xOffset + 1, -1, z), Vector3d(0, 0, 1), Vector2d(1, 0));
      mesh->addVertex(Vector3d(xOffset, 1, z), Vector3d(0, 0, 1), Vector2d(0, 1));
      mesh->addFace({0, 1, 2});
      return mesh;
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

    class PreparedFrontierTestBackend final : public WavefrontIntersectionBackend {
    public:
      explicit PreparedFrontierTestBackend(const Scene& scene)
          : m_compiledScene(IntersectionSceneCompiler().compile(scene)) {
      }

      using WavefrontIntersectionBackend::intersectPreparedAnyFrontier;
      using WavefrontIntersectionBackend::intersectPreparedClosestFrontier;

      const char* name() const override {
        return "prepared_frontier_test";
      }

      const CompiledIntersectionScene* compiledScene() const override {
        return &m_compiledScene;
      }

      const Primitive*
      intersectClosest(const Scene& /*scene*/, const Rayd& /*ray*/, HitPointInterval& /*hitPoints*/,
                       State& /*state*/,
                       WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return nullptr;
      }

      bool intersectAny(const Scene& /*scene*/, const Rayd& /*ray*/, double /*maxDistance*/,
                        State& /*state*/,
                        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return false;
      }

      PrimitivePacketHit4 intersectPacketClosest(
        const Scene& /*scene*/, const Ray4& /*rays*/, const PrimitivePacketState4& /*states*/,
        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return {};
      }

      PrimitivePacketHit8 intersectPacketClosest(
        const Scene& /*scene*/, const Ray8& /*rays*/, const PrimitivePacketState8& /*states*/,
        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return {};
      }

    private:
      CompiledIntersectionScene m_compiledScene;
    };

    class ShortDirectLightVisibilityBackend final : public WavefrontIntersectionBackend {
    public:
      const char* name() const override {
        return "short_direct_light_visibility";
      }

      const Primitive*
      intersectClosest(const Scene& /*scene*/, const Rayd& /*ray*/, HitPointInterval& /*hitPoints*/,
                       State& /*state*/,
                       WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return nullptr;
      }

      bool intersectAny(const Scene& /*scene*/, const Rayd& /*ray*/, double /*maxDistance*/,
                        State& /*state*/,
                        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return false;
      }

      WavefrontOcclusionFlags
      intersectAnyFrontier(const Scene&, const WavefrontAnyHitFrontier& frontier,
                           WavefrontIntersectionQueryTiming*) const override {
        if (frontier.rayCount() == 0) {
          return {};
        }
        return WavefrontOcclusionFlags(static_cast<std::size_t>(frontier.rayCount() - 1U), 0U);
      }

      PrimitivePacketHit4 intersectPacketClosest(
        const Scene& /*scene*/, const Ray4& /*rays*/, const PrimitivePacketState4& /*states*/,
        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return {};
      }

      PrimitivePacketHit8 intersectPacketClosest(
        const Scene& /*scene*/, const Ray8& /*rays*/, const PrimitivePacketState8& /*states*/,
        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return {};
      }
    };

    class ShortHostFrontierBatchBackend final : public WavefrontIntersectionBackend {
    public:
      const char* name() const override {
        return "short_host_frontier_batch";
      }

      const Primitive*
      intersectClosest(const Scene& /*scene*/, const Rayd& /*ray*/, HitPointInterval& /*hitPoints*/,
                       State& /*state*/,
                       WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return nullptr;
      }

      std::vector<WavefrontClosestHitResult>
      intersectClosestBatch(const Scene&, const std::vector<WavefrontClosestHitQuery>& queries,
                            WavefrontIntersectionQueryTiming*) const override {
        if (queries.empty()) {
          return {};
        }
        return std::vector<WavefrontClosestHitResult>(queries.size() - 1U);
      }

      bool intersectAny(const Scene& /*scene*/, const Rayd& /*ray*/, double /*maxDistance*/,
                        State& /*state*/,
                        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return false;
      }

      WavefrontOcclusionFlags intersectAnyBatch(const Scene&,
                                                const std::vector<WavefrontAnyHitQuery>& queries,
                                                WavefrontIntersectionQueryTiming*) const override {
        if (queries.empty()) {
          return {};
        }
        return WavefrontOcclusionFlags(queries.size() - 1U, 0U);
      }

      PrimitivePacketHit4 intersectPacketClosest(
        const Scene& /*scene*/, const Ray4& /*rays*/, const PrimitivePacketState4& /*states*/,
        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return {};
      }

      PrimitivePacketHit8 intersectPacketClosest(
        const Scene& /*scene*/, const Ray8& /*rays*/, const PrimitivePacketState8& /*states*/,
        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return {};
      }
    };

    enum class PackedRecordFailure { Short, DuplicateRayIndex };

    class MalformedPackedRecordBackend final : public WavefrontIntersectionBackend {
    public:
      MalformedPackedRecordBackend(const Scene& scene, PackedRecordFailure failure)
          : m_compiledScene(IntersectionSceneCompiler().compile(scene)),
            m_failure(failure) {
      }

      using WavefrontIntersectionBackend::intersectPreparedAnyBatch;
      using WavefrontIntersectionBackend::intersectPreparedClosestBatch;

      const char* name() const override {
        return "malformed_packed_record";
      }

      const CompiledIntersectionScene* compiledScene() const override {
        return &m_compiledScene;
      }

      const Primitive*
      intersectClosest(const Scene& /*scene*/, const Rayd& /*ray*/, HitPointInterval& /*hitPoints*/,
                       State& /*state*/,
                       WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return nullptr;
      }

      bool intersectAny(const Scene& /*scene*/, const Rayd& /*ray*/, double /*maxDistance*/,
                        State& /*state*/,
                        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return false;
      }

      PrimitivePacketHit4 intersectPacketClosest(
        const Scene& /*scene*/, const Ray4& /*rays*/, const PrimitivePacketState4& /*states*/,
        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return {};
      }

      PrimitivePacketHit8 intersectPacketClosest(
        const Scene& /*scene*/, const Ray8& /*rays*/, const PrimitivePacketState8& /*states*/,
        WavefrontIntersectionQueryTiming* /*timing*/ = nullptr) const override {
        return {};
      }

    protected:
      bool preparedPackedClosestHitAvailable() const override {
        return true;
      }

      std::vector<GpuIntersectionHitRecord> intersectPreparedPackedClosest(
        const std::vector<GpuIntersectionRay>& rays,
        WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        if (timing) {
          timing->recordExecutionPath("short_packed_closest");
        }
        if (rays.empty()) {
          return {};
        }
        if (m_failure == PackedRecordFailure::Short) {
          return std::vector<GpuIntersectionHitRecord>(rays.size() - 1);
        }
        return std::vector<GpuIntersectionHitRecord>(rays.size());
      }

      bool preparedPackedAnyHitAvailable() const override {
        return true;
      }

      std::vector<GpuIntersectionOcclusionRecord> intersectPreparedPackedAny(
        const std::vector<GpuIntersectionRay>& rays,
        WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        if (timing) {
          timing->recordExecutionPath("short_packed_any");
        }
        if (rays.empty()) {
          return {};
        }
        if (m_failure == PackedRecordFailure::Short) {
          return std::vector<GpuIntersectionOcclusionRecord>(rays.size() - 1);
        }
        return std::vector<GpuIntersectionOcclusionRecord>(rays.size());
      }

    private:
      CompiledIntersectionScene m_compiledScene;
      PackedRecordFailure m_failure;
    };

    class SyntheticPackedClosestHitFrontier final : public WavefrontClosestHitFrontier {
    public:
      SyntheticPackedClosestHitFrontier(std::vector<WavefrontClosestHitQuery> queries,
                                        std::vector<GpuIntersectionHitRecord> records)
          : m_records(std::move(records)) {
        m_states.reserve(queries.size());
        for (const WavefrontClosestHitQuery& query : queries) {
          m_states.push_back(query.state);
        }
      }

      std::uint64_t rayCount() const override {
        return static_cast<std::uint64_t>(m_states.size());
      }

      const char* residency() const override {
        return "synthetic_device";
      }

    protected:
      State* closestHitState(std::size_t rayIndex) const override {
        if (rayIndex >= m_states.size()) {
          return nullptr;
        }
        return m_states[rayIndex];
      }

      bool hasPackedClosestHitRays() const override {
        return true;
      }

      std::vector<GpuIntersectionHitRecord>
      intersectPackedClosest(const WavefrontIntersectionBackend& /*backend*/,
                             WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        if (timing) {
          timing->recordExecutionPath("synthetic_closest_frontier");
        }
        return m_records;
      }

    private:
      std::vector<State*> m_states;
      std::vector<GpuIntersectionHitRecord> m_records;
    };

    class SyntheticPackedAnyHitFrontier final : public WavefrontAnyHitFrontier {
    public:
      SyntheticPackedAnyHitFrontier(std::vector<WavefrontAnyHitQuery> queries,
                                    std::vector<GpuIntersectionOcclusionRecord> records)
          : m_records(std::move(records)) {
        m_states.reserve(queries.size());
        for (const WavefrontAnyHitQuery& query : queries) {
          m_states.push_back(query.state);
        }
      }

      std::uint64_t rayCount() const override {
        return static_cast<std::uint64_t>(m_states.size());
      }

      const char* residency() const override {
        return "synthetic_device";
      }

    protected:
      State* anyHitState(std::size_t rayIndex) const override {
        if (rayIndex >= m_states.size()) {
          return nullptr;
        }
        return m_states[rayIndex];
      }

      bool hasPackedAnyHitRays() const override {
        return true;
      }

      std::vector<GpuIntersectionOcclusionRecord>
      intersectPackedAny(const WavefrontIntersectionBackend& /*backend*/,
                         WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        if (timing) {
          timing->recordExecutionPath("synthetic_any_frontier");
        }
        return m_records;
      }

    private:
      std::vector<State*> m_states;
      std::vector<GpuIntersectionOcclusionRecord> m_records;
    };
  }

  TEST(WavefrontIntersectionBackend, AnyHitResultsUseConcreteByteFlags) {
    static_assert(std::is_same<WavefrontOcclusionFlags::value_type, unsigned char>::value,
                  "any-hit occlusion flags must stay byte-addressable");

    const WavefrontOcclusionFlags flags{1U, 0U};
    ASSERT_EQ(2u, flags.size());
    EXPECT_EQ(1U, flags[0]);
    EXPECT_EQ(0U, flags[1]);
  }

  TEST(WavefrontIntersectionBackend, SelectionContextDerivesExpectedRayCountFromQueryFamilies) {
    WavefrontIntersectionBackendSelectionContext context =
      WavefrontIntersectionBackendSelectionContext::fromExpectedQueryFamilies(2, 5);

    EXPECT_TRUE(context.hasExpectedQueryFamilies());
    EXPECT_EQ(7u, context.expectedRayCount);
    EXPECT_EQ(7u, context.effectiveExpectedRayCount());
    EXPECT_EQ(2u, context.expectedClosestHitRayCount);
    EXPECT_EQ(5u, context.expectedAnyHitRayCount);

    context.setExpectedQueryFamilies(11, 13);

    EXPECT_EQ(24u, context.expectedRayCount);
    EXPECT_EQ(24u, context.effectiveExpectedRayCount());
    EXPECT_EQ(11u, context.expectedClosestHitRayCount);
    EXPECT_EQ(13u, context.expectedAnyHitRayCount);
  }

  TEST(WavefrontIntersectionBackend, SelectionContextFallsBackToLegacyTotalWithoutFamilies) {
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 17;

    EXPECT_FALSE(context.hasExpectedQueryFamilies());
    EXPECT_EQ(17u, context.effectiveExpectedRayCount());
  }

  TEST(WavefrontIntersectionBackend, SelectionContextEffectiveRayCountPrefersFamilies) {
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 999;
    context.expectedClosestHitRayCount = 2;
    context.expectedAnyHitRayCount = 5;

    EXPECT_TRUE(context.hasExpectedQueryFamilies());
    EXPECT_EQ(7u, context.effectiveExpectedRayCount());
  }

  TEST(WavefrontIntersectionBackend, SelectionContextSaturatesExpectedRayCountFromFamilies) {
    constexpr std::uint64_t maxValue = std::numeric_limits<std::uint64_t>::max();

    EXPECT_EQ(maxValue, WavefrontIntersectionBackendSelectionContext::saturatedExpectedRayCount(
                          maxValue - 1, 2));

    const WavefrontIntersectionBackendSelectionContext context =
      WavefrontIntersectionBackendSelectionContext::fromExpectedQueryFamilies(maxValue - 3, 4);

    EXPECT_EQ(maxValue, context.expectedRayCount);
    EXPECT_EQ(maxValue, context.effectiveExpectedRayCount());
    EXPECT_EQ(maxValue - 3, context.expectedClosestHitRayCount);
    EXPECT_EQ(4u, context.expectedAnyHitRayCount);
  }

  TEST(WavefrontIntersectionQueryTiming, MergesExecutionPaths) {
    WavefrontIntersectionQueryTiming timing;
    WavefrontIntersectionQueryTiming packed;
    packed.uploadSeconds = 1.0;
    packed.recordExecutionPath("packed_cpu");
    packed.recordFallbackReason("platform dispatch failed");
    timing.add(packed);

    EXPECT_DOUBLE_EQ(1.0, timing.uploadSeconds);
    EXPECT_EQ("packed_cpu", timing.executionPath);
    EXPECT_EQ("platform dispatch failed", timing.fallbackReason);

    WavefrontIntersectionQueryTiming samePath;
    samePath.kernelSeconds = 2.0;
    samePath.recordExecutionPath("packed_cpu");
    samePath.recordFallbackReason("platform dispatch failed");
    timing.add(samePath);

    EXPECT_DOUBLE_EQ(2.0, timing.kernelSeconds);
    EXPECT_EQ("packed_cpu", timing.executionPath);
    EXPECT_EQ("platform dispatch failed", timing.fallbackReason);

    WavefrontIntersectionQueryTiming metal;
    metal.readbackSeconds = 3.0;
    metal.recordExecutionPath("metal");
    metal.recordFallbackReason("other dispatch failed");
    timing.add(metal);

    EXPECT_DOUBLE_EQ(3.0, timing.readbackSeconds);
    EXPECT_EQ("mixed", timing.executionPath);
    EXPECT_EQ("mixed", timing.fallbackReason);
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
    EXPECT_FALSE(backend.supportsPreparedRayBatchCompaction());
  }

  TEST(WavefrontFrontierCompaction, HostCompactionReportsRetainedMovedAndRemovedPaths) {
    WavefrontFrontierCompactionRequest request(5);
    request.setPathStateBytesPerPath(64);
    request.setPathStateResidency("host");
    request.retain(0);
    request.retain(2);
    request.retain(4);

    EXPECT_EQ(64u, request.pathStateBytesPerPath());
    EXPECT_EQ("host", request.pathStateResidency());
    EXPECT_EQ(5u * 64u, request.inputPathStateBytes());
    EXPECT_EQ(3u * 64u, request.retainedPathStateBytes());
    EXPECT_EQ(2u * 64u, request.removedPathStateBytes());
    EXPECT_EQ(3u * sizeof(std::uint32_t), request.retainedIndexBytes());

    const WavefrontFrontierCompactionResult compaction =
      WavefrontFrontierCompactionResult::hostCompaction(request);

    EXPECT_EQ(5u, compaction.inputPathCount());
    EXPECT_EQ(3u, compaction.retainedPathCount());
    EXPECT_EQ(2u, compaction.removedPathCount());
    EXPECT_DOUBLE_EQ(2.0 / 5.0, compaction.removedPathFraction());
    EXPECT_EQ(2u, compaction.movedPathCount());
    EXPECT_DOUBLE_EQ(2.0 / 3.0, compaction.movedRetainedPathFraction());
    EXPECT_EQ((std::vector<std::uint32_t>{0u, 2u, 4u}), compaction.retainedPathIndices());
    EXPECT_EQ(64u, compaction.pathStateBytesPerPath());
    EXPECT_EQ(5u * 64u, compaction.inputPathStateBytes());
    EXPECT_EQ(3u * 64u, compaction.retainedPathStateBytes());
    EXPECT_EQ(2u * 64u, compaction.removedPathStateBytes());
    EXPECT_EQ(3u * sizeof(std::uint32_t), compaction.retainedIndexBytes());
    EXPECT_EQ("host", compaction.executionPath());
    EXPECT_EQ("host", compaction.pathStateResidency());
  }

  TEST(WavefrontFrontierCompaction, CarriesPathStateResidencyThroughResult) {
    WavefrontFrontierCompactionRequest request(3);
    request.setPathStateResidency("device_test");
    request.retain(0);
    request.retain(2);

    const WavefrontFrontierCompactionResult compaction =
      WavefrontFrontierCompactionResult::hostCompaction(request);

    EXPECT_EQ("host", compaction.executionPath());
    EXPECT_EQ("device_test", compaction.pathStateResidency());
  }

  TEST(WavefrontFrontierCompaction, CarriesBackendTimingThroughResult) {
    WavefrontFrontierCompactionResult compaction =
      WavefrontFrontierCompactionResult::fromRetainedPathIndices(3, {0u, 2u}, "metal");
    WavefrontFrontierCompactionTiming timing;
    timing.uploadSeconds = 0.004;
    timing.kernelSeconds = 0.005;
    timing.readbackSeconds = 0.006;

    compaction.setTiming(timing);

    EXPECT_DOUBLE_EQ(0.004, compaction.timing().uploadSeconds);
    EXPECT_DOUBLE_EQ(0.005, compaction.timing().kernelSeconds);
    EXPECT_DOUBLE_EQ(0.006, compaction.timing().readbackSeconds);
  }

  TEST(WavefrontFrontierCompaction, NormalizesEmptyResultLabels) {
    const WavefrontFrontierCompactionResult compaction =
      WavefrontFrontierCompactionResult::fromRetainedPathIndices(2, {0u}, "",
                                                                 /*pathStateBytesPerPath=*/32u, "");

    EXPECT_EQ("unknown", compaction.executionPath());
    EXPECT_EQ("unknown", compaction.pathStateResidency());
  }

  TEST(WavefrontFrontierCompaction, PathStateByteEstimatesSaturate) {
    WavefrontFrontierCompactionRequest request(2);
    request.setPathStateBytesPerPath(std::numeric_limits<std::uint64_t>::max());
    request.retain(0);

    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), request.inputPathStateBytes());
    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), request.retainedPathStateBytes());
    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), request.removedPathStateBytes());

    const WavefrontFrontierCompactionResult compaction =
      WavefrontFrontierCompactionResult::hostCompaction(request);

    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), compaction.inputPathStateBytes());
    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), compaction.retainedPathStateBytes());
    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), compaction.removedPathStateBytes());
  }

  TEST(WavefrontFrontierCompaction, EmptyCompactionHasNoRemovedFraction) {
    const WavefrontFrontierCompactionRequest request(0);

    const WavefrontFrontierCompactionResult compaction =
      WavefrontFrontierCompactionResult::hostCompaction(request);

    EXPECT_EQ(0u, compaction.inputPathCount());
    EXPECT_EQ(0u, compaction.retainedPathCount());
    EXPECT_EQ(0u, compaction.removedPathCount());
    EXPECT_EQ(0u, compaction.retainedIndexBytes());
    EXPECT_DOUBLE_EQ(0.0, compaction.movedRetainedPathFraction());
    EXPECT_DOUBLE_EQ(0.0, compaction.removedPathFraction());
  }

  TEST(WavefrontFrontierCompaction, RejectsOutOfRangeRetainedPaths) {
    WavefrontFrontierCompactionRequest request(2);

    EXPECT_THROW(request.retain(2), std::out_of_range);
    EXPECT_THROW(
      (void)WavefrontFrontierCompactionResult::fromRetainedPathIndices(2, {0u, 2u}, "host"),
      std::out_of_range);
  }

  TEST(WavefrontFrontierCompaction, RejectsRetainedPathIndicesOutsideGpuIndexRange) {
    if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t)) {
      const std::size_t tooLarge =
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1u;
      WavefrontFrontierCompactionRequest request(tooLarge + 1u);

      EXPECT_THROW(request.retain(tooLarge), std::overflow_error);
    }
  }

  TEST(WavefrontFrontierCompaction, RejectsUnsortedRetainedPaths) {
    WavefrontFrontierCompactionRequest request(4);
    request.retain(2);

    EXPECT_THROW(request.retain(1), std::invalid_argument);
    EXPECT_THROW(request.retain(2), std::invalid_argument);
    EXPECT_THROW(
      (void)WavefrontFrontierCompactionResult::fromRetainedPathIndices(4, {0u, 3u, 2u}, "host"),
      std::invalid_argument);
  }

  TEST(WavefrontIntersectionBackend, CpuBackendCompactsFrontiersOnHost) {
    WavefrontFrontierCompactionRequest request(3);
    request.retain(1);
    request.retain(2);

    const WavefrontFrontierCompactionResult compaction =
      CpuWavefrontIntersectionBackend::instance().compactFrontier(request);

    EXPECT_EQ(3u, compaction.inputPathCount());
    EXPECT_EQ(2u, compaction.retainedPathCount());
    EXPECT_EQ(1u, compaction.removedPathCount());
    EXPECT_EQ(2u, compaction.movedPathCount());
    EXPECT_EQ(2u * sizeof(std::uint32_t), compaction.retainedIndexBytes());
    EXPECT_EQ("host", compaction.executionPath());
  }

  TEST(WavefrontIntersectionBackend, CpuBackendIntersectsHostClosestHitFrontier) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    State state;
    std::vector<WavefrontClosestHitQuery> queries;
    queries.push_back(
      WavefrontClosestHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), &state});

    const auto frontier =
      CpuWavefrontIntersectionBackend::instance().createClosestHitFrontier(std::move(queries));

    ASSERT_NE(nullptr, frontier);
    EXPECT_EQ(1u, frontier->rayCount());
    EXPECT_STREQ("host", frontier->residency());
    EXPECT_EQ(0u, frontier->packedRayBytes());
    EXPECT_EQ(0u, frontier->hostPackedRayBytes());
    EXPECT_EQ(sizeof(WavefrontClosestHitQuery), frontier->hostQueryBytes());

    WavefrontIntersectionQueryTiming timing;
    const std::vector<WavefrontClosestHitResult> hits =
      CpuWavefrontIntersectionBackend::instance().intersectClosestFrontier(scene, *frontier,
                                                                           &timing);

    ASSERT_EQ(1u, hits.size());
    EXPECT_TRUE(hits.front().hit());
    EXPECT_EQ("runtime_scene", timing.executionPath);
  }

  TEST(WavefrontIntersectionBackend, CpuBackendIntersectsHostAnyHitFrontier) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    State state;
    std::vector<WavefrontAnyHitQuery> queries;
    queries.push_back(
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 4.0, &state});

    const auto frontier =
      CpuWavefrontIntersectionBackend::instance().createAnyHitFrontier(std::move(queries));

    ASSERT_NE(nullptr, frontier);
    EXPECT_EQ(1u, frontier->rayCount());
    EXPECT_STREQ("host", frontier->residency());
    EXPECT_EQ(0u, frontier->packedRayBytes());
    EXPECT_EQ(0u, frontier->hostPackedRayBytes());
    EXPECT_EQ(sizeof(WavefrontAnyHitQuery), frontier->hostQueryBytes());

    WavefrontIntersectionQueryTiming timing;
    const WavefrontOcclusionFlags occluded =
      CpuWavefrontIntersectionBackend::instance().intersectAnyFrontier(scene, *frontier, &timing);

    ASSERT_EQ(1u, occluded.size());
    EXPECT_TRUE(occluded.front());
    EXPECT_EQ("runtime_scene", timing.executionPath);
  }

  TEST(WavefrontIntersectionBackend, HostClosestHitFrontierRejectsMismatchedResultCount) {
    Scene scene;
    ShortHostFrontierBatchBackend backend;
    State firstState;
    State secondState;
    std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), &firstState},
      WavefrontClosestHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), &secondState}};
    auto frontier = backend.createClosestHitFrontier(std::move(queries));

    ASSERT_NE(nullptr, frontier);
    EXPECT_THROW((void)backend.intersectClosestFrontier(scene, *frontier), std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, HostAnyHitFrontierRejectsMismatchedResultCount) {
    Scene scene;
    ShortHostFrontierBatchBackend backend;
    State firstState;
    State secondState;
    std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 4.0, &firstState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &secondState}};
    auto frontier = backend.createAnyHitFrontier(std::move(queries));

    ASSERT_NE(nullptr, frontier);
    EXPECT_THROW((void)backend.intersectAnyFrontier(scene, *frontier), std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, DirectLightVisibilityBatchRejectsMismatchedFrontierResults) {
    Scene scene;
    ShortDirectLightVisibilityBackend backend;
    State firstState;
    State secondState;
    std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 4.0, &firstState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &secondState}};

    EXPECT_THROW((void)backend.resolveDirectLightVisibilityBatch(scene, std::move(queries)),
                 std::logic_error);
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

  TEST(WavefrontIntersectionBackend, CompiledSceneDiagnosticsDoNotInventUploadBuffers) {
    auto instance = std::make_shared<Instance>(nullptr);
    instance->setName("render empty instance");
    Scene scene;
    scene.add(instance);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const WavefrontIntersectionSceneDiagnostics diagnostics =
      WavefrontIntersectionSceneDiagnostics::fromCompiledScene(compiled);

    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(1u, diagnostics.bvhNodes);
    EXPECT_EQ(1u, diagnostics.primitives);
    EXPECT_EQ(1u, diagnostics.unsupportedPrimitives);
    ASSERT_EQ(1u, diagnostics.unsupportedReasons.size());
    EXPECT_EQ(1u, diagnostics.unsupportedReasons.at(
                    "empty instance is not supported by GPU intersection scene compiler"));
    EXPECT_EQ(0u, diagnostics.uploadBytes);
    EXPECT_FALSE(diagnostics.triangleClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.basicHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedAnyHitKernelEligible);
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
    const MetalWavefrontSmokeKernel kernel;
    const std::string fallback = backend.fallbackReason();
    if (!kernel.deviceAvailable()) {
      ASSERT_FALSE(kernel.deviceUnavailableReason().empty());
      EXPECT_NE(std::string::npos, fallback.find(kernel.deviceUnavailableReason()));
    } else if (!kernel.renderPathAvailable()) {
      ASSERT_FALSE(kernel.renderPathUnavailableReason().empty());
      EXPECT_NE(std::string::npos, fallback.find(kernel.renderPathUnavailableReason()));
    }
#endif
    EXPECT_EQ(nullptr, backend.compiledScene());
    expectUnavailablePlatformFallback(backend, "Metal", "runtime_scene", "metal");
  }

  TEST(WavefrontIntersectionBackend, VulkanStubReportsUnavailableCpuFallback) {
    const auto& backend = VulkanWavefrontIntersectionBackend::instance();

    EXPECT_STREQ("vulkan", backend.platformName());
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    EXPECT_EQ(VulkanWavefrontSmokeKernel().deviceAvailable(), backend.isAvailable());
    EXPECT_EQ(VulkanWavefrontSmokeKernel().renderPathAvailable(),
              backend.platformGpuRenderPathAvailable());
#else
    EXPECT_FALSE(backend.isAvailable());
    EXPECT_FALSE(backend.platformGpuRenderPathAvailable());
#endif
    const VulkanWavefrontSmokeKernel kernel;
    const std::string fallback = backend.fallbackReason();
    if (!kernel.deviceAvailable()) {
      ASSERT_FALSE(kernel.deviceUnavailableReason().empty());
      EXPECT_NE(std::string::npos, fallback.find(kernel.deviceUnavailableReason()));
    } else if (!kernel.renderPathAvailable()) {
      ASSERT_FALSE(kernel.renderPathUnavailableReason().empty());
      EXPECT_NE(std::string::npos, fallback.find(kernel.renderPathUnavailableReason()));
    }
    EXPECT_EQ(nullptr, backend.compiledScene());
    expectUnavailablePlatformFallback(backend, "Vulkan", "runtime_scene", "vulkan");
  }

  TEST(VulkanWavefrontSmokeKernel, ReportsUnavailableWhenDisabled) {
    VulkanWavefrontSmokeKernel kernel;
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    EXPECT_NO_THROW((void)kernel.deviceAvailable());
    if (kernel.deviceAvailable()) {
      EXPECT_TRUE(kernel.deviceUnavailableReason().empty());
    } else {
      EXPECT_FALSE(kernel.deviceUnavailableReason().empty());
    }
    if (kernel.renderPathAvailable()) {
      EXPECT_TRUE(kernel.renderPathUnavailableReason().empty());
    } else {
      EXPECT_FALSE(kernel.renderPathUnavailableReason().empty());
    }
#else
    EXPECT_FALSE(kernel.deviceAvailable());
    EXPECT_FALSE(kernel.renderPathAvailable());
    EXPECT_NE(std::string::npos, kernel.deviceUnavailableReason().find("not enabled"));
    EXPECT_NE(std::string::npos, kernel.renderPathUnavailableReason().find("not enabled"));
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
    EXPECT_EQ(65536u, decision.minimumExpectedRayCount);
    EXPECT_NE(std::string::npos, decision.reason.find("unavailable"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresRenderPathAvailability) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, false, supportedPackedDiagnostics(), context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_EQ(65536u, decision.minimumExpectedRayCount);
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
    EXPECT_EQ(65536u, decision.minimumExpectedRayCount);
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
    EXPECT_EQ(64u, decision.minimumExpectedRayCount);
    EXPECT_NE(std::string::npos, decision.reason.find("below GPU threshold"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRejectsSmallWorkloadBeforeSceneCompile) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 63;
    context.minimumGpuRayCount = 64;

    const std::optional<WavefrontIntersectionBackendAutoSelectionDecision> decision =
      policy.decideBeforeSceneCompile(context);

    ASSERT_TRUE(decision.has_value());
    EXPECT_FALSE(decision->useGpu);
    EXPECT_EQ(64u, decision->minimumExpectedRayCount);
    EXPECT_EQ(0u, decision->estimatedQueryTransferBytes);
    EXPECT_NE(std::string::npos, decision->reason.find("expected ray count 63"));
    EXPECT_NE(std::string::npos, decision->reason.find("fixed GPU threshold 64"));
    EXPECT_NE(std::string::npos, decision->reason.find("before scene compilation"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyDefersToSceneDiagnosticsAtFixedThreshold) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 64;
    context.minimumGpuRayCount = 64;

    EXPECT_FALSE(policy.decideBeforeSceneCompile(context).has_value());
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyPrecompileDecisionUsesFamilyDerivedRayCount) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 999;
    context.expectedClosestHitRayCount = 3;
    context.expectedAnyHitRayCount = 4;
    context.minimumGpuRayCount = 8;

    const std::optional<WavefrontIntersectionBackendAutoSelectionDecision> decision =
      policy.decideBeforeSceneCompile(context);

    ASSERT_TRUE(decision.has_value());
    EXPECT_FALSE(decision->useGpu);
    EXPECT_EQ(8u, decision->minimumExpectedRayCount);
    EXPECT_NE(std::string::npos, decision->reason.find("expected ray count 7"));
    EXPECT_NE(std::string::npos, decision->reason.find("fixed GPU threshold 8"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyUsesFamilyDerivedExpectedRayCount) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1;
    context.expectedClosestHitRayCount = 40;
    context.expectedAnyHitRayCount = 24;
    context.minimumGpuRayCount = 64;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, supportedPackedDiagnostics(), context);

    EXPECT_TRUE(decision.useGpu);
    EXPECT_EQ(64u, decision.minimumExpectedRayCount);
    EXPECT_NE(std::string::npos, decision.reason.find("auto selected GPU"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyReportsFamilyDerivedExpectedRayCount) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 999;
    context.expectedClosestHitRayCount = 3;
    context.expectedAnyHitRayCount = 4;
    context.minimumGpuRayCount = 8;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, supportedPackedDiagnostics(), context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_NE(std::string::npos, decision.reason.find("expected ray count 7"));
    EXPECT_NE(std::string::npos, decision.reason.find("below GPU threshold 8"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyEstimatesSupportedQueryTransferBytes) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 3;

    const std::uint64_t expectedBytes =
      context.expectedRayCount *
      static_cast<std::uint64_t>(sizeof(GpuIntersectionRay) + sizeof(GpuIntersectionHitRecord));

    EXPECT_EQ(expectedBytes,
              policy.estimatedQueryTransferBytes(supportedPackedDiagnostics(), context));

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, supportedPackedDiagnostics(), context);

    EXPECT_EQ(expectedBytes, decision.estimatedQueryTransferBytes);
    EXPECT_NE(std::string::npos, decision.reason.find("estimated query transfer"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyEstimatesClosestHitAndAnyHitTransferBytes) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 7;
    context.expectedClosestHitRayCount = 2;
    context.expectedAnyHitRayCount = 5;

    const std::uint64_t expectedBytes =
      context.expectedClosestHitRayCount *
        static_cast<std::uint64_t>(sizeof(GpuIntersectionRay) + sizeof(GpuIntersectionHitRecord)) +
      context.expectedAnyHitRayCount *
        static_cast<std::uint64_t>(sizeof(GpuIntersectionRay) +
                                   sizeof(GpuIntersectionOcclusionRecord));

    EXPECT_EQ(expectedBytes,
              policy.estimatedQueryTransferBytes(supportedPackedDiagnostics(), context));

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, supportedPackedDiagnostics(), context);

    EXPECT_EQ(expectedBytes, decision.estimatedQueryTransferBytes);
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyReportsNoQueryTransferForUnsupportedScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 3;
    WavefrontIntersectionSceneDiagnostics diagnostics = supportedPackedDiagnostics();
    diagnostics.packedAnyHitKernelEligible = false;

    EXPECT_EQ(0u, policy.estimatedQueryTransferBytes(diagnostics, context));

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, diagnostics, context);

    EXPECT_EQ(0u, decision.estimatedQueryTransferBytes);
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
    EXPECT_EQ(128u, decision.minimumExpectedRayCount);
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
    EXPECT_EQ(128u, decision.minimumExpectedRayCount);
    EXPECT_NE(std::string::npos, decision.reason.find("auto selected GPU"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresPackedClosestHitEligibleScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;
    WavefrontIntersectionSceneDiagnostics diagnostics = supportedPackedDiagnostics();
    diagnostics.packedClosestHitKernelEligible = false;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, diagnostics, context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_EQ(65536u, decision.minimumExpectedRayCount);
    EXPECT_NE(std::string::npos, decision.reason.find("packed closest-hit eligible"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresPlatformBasicHitEligibleScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;
    WavefrontIntersectionSceneDiagnostics diagnostics = supportedPackedDiagnostics();
    diagnostics.basicHitKernelEligible = false;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, diagnostics, context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_EQ(65536u, decision.minimumExpectedRayCount);
    EXPECT_NE(std::string::npos, decision.reason.find("platform basic-hit eligible"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicyRequiresPackedAnyHitEligibleScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;
    WavefrontIntersectionSceneDiagnostics diagnostics = supportedPackedDiagnostics();
    diagnostics.packedAnyHitKernelEligible = false;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, diagnostics, context);

    EXPECT_FALSE(decision.useGpu);
    EXPECT_EQ(65536u, decision.minimumExpectedRayCount);
    EXPECT_NE(std::string::npos, decision.reason.find("packed any-hit eligible"));
  }

  TEST(WavefrontIntersectionBackend, AutoPolicySelectsGpuForAvailableLargeSupportedScene) {
    const WavefrontIntersectionBackendAutoSelectionPolicy policy;
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 64;
    context.minimumGpuRayCount = 64;

    const WavefrontIntersectionBackendAutoSelectionDecision decision =
      policy.decide(true, true, supportedPackedDiagnostics(), context);

    EXPECT_TRUE(decision.useGpu);
    EXPECT_EQ(64u, decision.minimumExpectedRayCount);
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

  TEST(VulkanWavefrontSmokeKernel, RunsBasicClosestHitKernelWhenEnabled) {
    VulkanWavefrontSmokeKernel kernel;

    Scene scene;
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 6), Vector3d(1, -1, 6), Vector3d(0, 1, 6)));
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 2), Vector3d(1, -1, 2), Vector3d(0, 1, 2)));
    auto mesh = triangleMeshAt(4.0, 3.0);
    scene.add(std::make_shared<FlatMeshTriangle>(mesh.get(), 0, 1, 2));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 7),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 8),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 9, 0.0,
                                           1.0),
    };

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }

    const std::vector<GpuIntersectionHitRecord> expected =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);

    const VulkanWavefrontClosestHitKernelResult actual =
      kernel.runTimedBasicClosestHitKernel(buffers, rays);

    ASSERT_EQ(expected.size(), actual.hits.size());
    EXPECT_EQ(std::string("vulkan"), actual.timing.executionPath);
    for (std::size_t index = 0; index != expected.size(); ++index) {
      expectGpuHitRecordNear(actual.hits[index], expected[index]);
    }
#else
    EXPECT_THROW((void)kernel.runBasicClosestHitKernel(buffers, rays), std::runtime_error);
#endif
  }

  TEST(VulkanWavefrontSmokeKernel, RunsBasicAnyHitKernelWhenEnabled) {
    VulkanWavefrontSmokeKernel kernel;

    Scene scene;
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 6), Vector3d(1, -1, 6), Vector3d(0, 1, 6)));
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 2), Vector3d(1, -1, 2), Vector3d(0, 1, 2)));
    auto mesh = triangleMeshAt(4.0, 3.0);
    scene.add(std::make_shared<FlatMeshTriangle>(mesh.get(), 0, 1, 2));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 7, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 8, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 9, 0.0,
                                           1.0),
    };

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }

    const std::vector<GpuIntersectionOcclusionRecord> expected =
      GpuIntersectionIntersector().intersectAny(buffers, rays);

    const VulkanWavefrontAnyHitKernelResult actual =
      kernel.runTimedBasicAnyHitKernel(buffers, rays);

    ASSERT_EQ(expected.size(), actual.records.size());
    EXPECT_EQ(std::string("vulkan"), actual.timing.executionPath);
    for (std::size_t index = 0; index != expected.size(); ++index) {
      EXPECT_EQ(expected[index].occluded, actual.records[index].occluded);
      EXPECT_EQ(expected[index].rayIndex, actual.records[index].rayIndex);
    }
#else
    EXPECT_THROW((void)kernel.runBasicAnyHitKernel(buffers, rays), std::runtime_error);
#endif
  }

  TEST(VulkanWavefrontSmokeKernel, RunsSphereBasicHitKernelsWhenEnabled) {
    VulkanWavefrontSmokeKernel kernel;

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 11),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 12),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 13, 0.0,
                                           1.0),
    };

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }

    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);
    const VulkanWavefrontClosestHitKernelResult actualClosest =
      kernel.runTimedBasicClosestHitKernel(buffers, rays);

    ASSERT_EQ(expectedClosest.size(), actualClosest.hits.size());
    EXPECT_EQ(std::string("vulkan"), actualClosest.timing.executionPath);
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest[index]);
    }

    const std::vector<GpuIntersectionOcclusionRecord> expectedAny =
      GpuIntersectionIntersector().intersectAny(buffers, rays);
    const VulkanWavefrontAnyHitKernelResult actualAny =
      kernel.runTimedBasicAnyHitKernel(buffers, rays);

    ASSERT_EQ(expectedAny.size(), actualAny.records.size());
    EXPECT_EQ(std::string("vulkan"), actualAny.timing.executionPath);
    for (std::size_t index = 0; index != expectedAny.size(); ++index) {
      EXPECT_EQ(expectedAny[index].occluded, actualAny.records[index].occluded);
      EXPECT_EQ(expectedAny[index].rayIndex, actualAny.records[index].rayIndex);
    }
#else
    EXPECT_THROW((void)kernel.runBasicClosestHitKernel(buffers, rays), std::runtime_error);
    EXPECT_THROW((void)kernel.runBasicAnyHitKernel(buffers, rays), std::runtime_error);
#endif
  }

  TEST(VulkanWavefrontSmokeKernel, RunsOpenCylinderBasicHitKernelsWhenEnabled) {
    VulkanWavefrontSmokeKernel kernel;

    Scene scene;
    scene.add(std::make_shared<OpenCylinder>(1.0, 2.0));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_EQ(1u, buffers.openCylinders.size());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 41, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(1, 0, 0)), 42, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 2, -3, 1), Vector3d(0, 0, 1)), 43, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 44, 0.0,
                                           1.0),
    };

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }

    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);
    const VulkanWavefrontClosestHitKernelResult actualClosest =
      kernel.runTimedBasicClosestHitKernel(buffers, rays);

    ASSERT_EQ(expectedClosest.size(), actualClosest.hits.size());
    EXPECT_EQ(std::string("vulkan"), actualClosest.timing.executionPath);
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest[index]);
    }

    const std::vector<GpuIntersectionOcclusionRecord> expectedAny =
      GpuIntersectionIntersector().intersectAny(buffers, rays);
    const VulkanWavefrontAnyHitKernelResult actualAny =
      kernel.runTimedBasicAnyHitKernel(buffers, rays);

    ASSERT_EQ(expectedAny.size(), actualAny.records.size());
    EXPECT_EQ(std::string("vulkan"), actualAny.timing.executionPath);
    for (std::size_t index = 0; index != expectedAny.size(); ++index) {
      EXPECT_EQ(expectedAny[index].occluded, actualAny.records[index].occluded);
      EXPECT_EQ(expectedAny[index].rayIndex, actualAny.records[index].rayIndex);
    }
#else
    EXPECT_THROW((void)kernel.runBasicClosestHitKernel(buffers, rays), std::runtime_error);
    EXPECT_THROW((void)kernel.runBasicAnyHitKernel(buffers, rays), std::runtime_error);
#endif
  }

  TEST(VulkanWavefrontSmokeKernel, RunsTorusBasicHitKernelsWhenEnabled) {
    VulkanWavefrontSmokeKernel kernel;

    Scene scene;
    scene.add(std::make_shared<Torus>(2.0, 0.5));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_EQ(1u, buffers.tori.size());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -4, 1), Vector3d(0, 0, 1)), 45, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)), 46, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(2, 0, -2, 1), Vector3d(0, 0, 1)), 47, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -4, 1), Vector3d(0, 0, 1)), 48, 0.0,
                                           1.0),
    };

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }

    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);
    const VulkanWavefrontClosestHitKernelResult actualClosest =
      kernel.runTimedBasicClosestHitKernel(buffers, rays);

    ASSERT_EQ(expectedClosest.size(), actualClosest.hits.size());
    EXPECT_EQ(std::string("vulkan"), actualClosest.timing.executionPath);
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest[index]);
    }

    const std::vector<GpuIntersectionOcclusionRecord> expectedAny =
      GpuIntersectionIntersector().intersectAny(buffers, rays);
    const VulkanWavefrontAnyHitKernelResult actualAny =
      kernel.runTimedBasicAnyHitKernel(buffers, rays);

    ASSERT_EQ(expectedAny.size(), actualAny.records.size());
    EXPECT_EQ(std::string("vulkan"), actualAny.timing.executionPath);
    for (std::size_t index = 0; index != expectedAny.size(); ++index) {
      EXPECT_EQ(expectedAny[index].occluded, actualAny.records[index].occluded);
      EXPECT_EQ(expectedAny[index].rayIndex, actualAny.records[index].rayIndex);
    }
#else
    EXPECT_THROW((void)kernel.runBasicClosestHitKernel(buffers, rays), std::runtime_error);
    EXPECT_THROW((void)kernel.runBasicAnyHitKernel(buffers, rays), std::runtime_error);
#endif
  }

  TEST(VulkanWavefrontSmokeKernel, RunsExactPrimitiveBasicHitKernelsWhenEnabled) {
    VulkanWavefrontSmokeKernel kernel;

    Scene scene;
    scene.add(std::make_shared<Plane>(Vector3d(0, 0, 2), -18.0));
    scene.add(std::make_shared<Rectangle>(Vector3d(-1, -1, 5), Vector3d(2, 0, 0), Vector3d(0, 2, 0),
                                          Vector3d(0, 0, 2)));
    scene.add(std::make_shared<Disk>(Vector3d(3, 0, 4), Vector3d(0, 0, 2), 1.0));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 3, 0, 1), Vector3d(0, 0, 1)), 21, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 22, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(3, 0, 0, 1), Vector3d(0, 0, 1)), 23, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(6, 0, 0, 1), Vector3d(0, 0, 1)), 24, 0.0,
                                           3.5),
    };

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }

    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);
    const VulkanWavefrontClosestHitKernelResult actualClosest =
      kernel.runTimedBasicClosestHitKernel(buffers, rays);

    ASSERT_EQ(expectedClosest.size(), actualClosest.hits.size());
    EXPECT_EQ(std::string("vulkan"), actualClosest.timing.executionPath);
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest[index]);
    }

    const std::vector<GpuIntersectionOcclusionRecord> expectedAny =
      GpuIntersectionIntersector().intersectAny(buffers, rays);
    const VulkanWavefrontAnyHitKernelResult actualAny =
      kernel.runTimedBasicAnyHitKernel(buffers, rays);

    ASSERT_EQ(expectedAny.size(), actualAny.records.size());
    EXPECT_EQ(std::string("vulkan"), actualAny.timing.executionPath);
    for (std::size_t index = 0; index != expectedAny.size(); ++index) {
      EXPECT_EQ(expectedAny[index].occluded, actualAny.records[index].occluded);
      EXPECT_EQ(expectedAny[index].rayIndex, actualAny.records[index].rayIndex);
    }
#else
    EXPECT_THROW((void)kernel.runBasicClosestHitKernel(buffers, rays), std::runtime_error);
    EXPECT_THROW((void)kernel.runBasicAnyHitKernel(buffers, rays), std::runtime_error);
#endif
  }

  TEST(VulkanWavefrontSmokeKernel, RunsStaticTransformBasicHitKernelsWhenEnabled) {
    VulkanWavefrontSmokeKernel kernel;

    auto triangleMaterial =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::red()));
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    auto triangleInstance = std::make_shared<Instance>(triangle);
    triangleInstance->setMaterial(triangleMaterial);
    triangleInstance->setMatrix(Matrix4d::translate(0, 0, 2));

    auto sphereMaterial =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto sphereInstance = std::make_shared<Instance>(sphere);
    sphereInstance->setMaterial(sphereMaterial);
    sphereInstance->setMatrix(Matrix4d::translate(3, 0, 4));

    Scene scene;
    scene.add(triangleInstance);
    scene.add(sphereInstance);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_GT(buffers.transforms.size(), 2u);
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -2, 1), Vector3d(0, 0, 1)), 31, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(3, 0, 0, 1), Vector3d(0, 0, 1)), 32, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(6, 0, 0, 1), Vector3d(0, 0, 1)), 33, 0.0,
                                           10.0),
    };

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }

    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);
    ASSERT_TRUE(expectedClosest[0].hit);
    ASSERT_TRUE(expectedClosest[1].hit);
    ASSERT_LT(expectedClosest[0].object, compiled.objects().size());
    ASSERT_LT(expectedClosest[1].object, compiled.objects().size());
    ASSERT_LT(expectedClosest[0].material, compiled.materials().size());
    ASSERT_LT(expectedClosest[1].material, compiled.materials().size());
    EXPECT_EQ(triangleInstance.get(), compiled.objects()[expectedClosest[0].object]);
    EXPECT_EQ(sphereInstance.get(), compiled.objects()[expectedClosest[1].object]);
    EXPECT_EQ(triangleMaterial, compiled.materials()[expectedClosest[0].material]);
    EXPECT_EQ(sphereMaterial, compiled.materials()[expectedClosest[1].material]);
    const VulkanWavefrontClosestHitKernelResult actualClosest =
      kernel.runTimedBasicClosestHitKernel(buffers, rays);

    ASSERT_EQ(expectedClosest.size(), actualClosest.hits.size());
    EXPECT_EQ(std::string("vulkan"), actualClosest.timing.executionPath);
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest[index]);
    }

    const std::vector<GpuIntersectionOcclusionRecord> expectedAny =
      GpuIntersectionIntersector().intersectAny(buffers, rays);
    const VulkanWavefrontAnyHitKernelResult actualAny =
      kernel.runTimedBasicAnyHitKernel(buffers, rays);

    ASSERT_EQ(expectedAny.size(), actualAny.records.size());
    EXPECT_EQ(std::string("vulkan"), actualAny.timing.executionPath);
    for (std::size_t index = 0; index != expectedAny.size(); ++index) {
      EXPECT_EQ(expectedAny[index].occluded, actualAny.records[index].occluded);
      EXPECT_EQ(expectedAny[index].rayIndex, actualAny.records[index].rayIndex);
    }
#else
    EXPECT_THROW((void)kernel.runBasicClosestHitKernel(buffers, rays), std::runtime_error);
    EXPECT_THROW((void)kernel.runBasicAnyHitKernel(buffers, rays), std::runtime_error);
#endif
  }

  TEST(VulkanWavefrontPreparedScene, SupportsConcurrentClosestAndAnyHitQueries) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }
    if (!kernel.renderPathAvailable()) {
      GTEST_SKIP() << kernel.renderPathUnavailableReason();
    }

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 6), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> closestRays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 51),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 52),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 53, 0.0,
                                           2.5),
    };
    const std::vector<GpuIntersectionRay> anyRays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 61, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 62, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 63, 0.0,
                                           2.5),
    };
    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, closestRays);
    const std::vector<GpuIntersectionOcclusionRecord> expectedAny =
      GpuIntersectionIntersector().intersectAny(buffers, anyRays);

    const VulkanWavefrontPreparedScene prepared(buffers);
    auto closestFuture = std::async(
      std::launch::async, [&] { return prepared.runTimedBasicClosestHitKernel(closestRays); });
    auto anyFuture =
      std::async(std::launch::async, [&] { return prepared.runTimedBasicAnyHitKernel(anyRays); });

    const VulkanWavefrontClosestHitKernelResult closest = closestFuture.get();
    const VulkanWavefrontAnyHitKernelResult any = anyFuture.get();

    ASSERT_EQ(expectedClosest.size(), closest.hits.size());
    EXPECT_EQ(std::string("vulkan"), closest.timing.executionPath);
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(closest.hits[index], expectedClosest[index]);
    }

    ASSERT_EQ(expectedAny.size(), any.records.size());
    EXPECT_EQ(std::string("vulkan"), any.timing.executionPath);
    for (std::size_t index = 0; index != expectedAny.size(); ++index) {
      EXPECT_EQ(expectedAny[index].occluded, any.records[index].occluded);
      EXPECT_EQ(expectedAny[index].rayIndex, any.records[index].rayIndex);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront backend is disabled";
#endif
  }

  TEST(VulkanWavefrontPreparedScene, ReusesPreparedRayBatchAcrossClosestAndAnyHitQueries) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }
    if (!kernel.renderPathAvailable()) {
      GTEST_SKIP() << kernel.renderPathUnavailableReason();
    }

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 6), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 91),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 92),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 93, 0.0,
                                           2.5),
    };

    const VulkanWavefrontPreparedScene prepared(buffers);
    const std::shared_ptr<const VulkanWavefrontPreparedRayBatch> rayBatch =
      prepared.prepareRays(rays);
    ASSERT_NE(nullptr, rayBatch);
    EXPECT_EQ(rays.size(), rayBatch->rayCount());
    EXPECT_EQ(rays.size() * sizeof(GpuIntersectionRay), rayBatch->packedRayBytes());

    const VulkanWavefrontClosestHitKernelResult expectedClosest =
      prepared.runTimedBasicClosestHitKernel(rays);
    const VulkanWavefrontClosestHitKernelResult actualClosest =
      prepared.runTimedBasicClosestHitKernel(*rayBatch);

    ASSERT_EQ(expectedClosest.hits.size(), actualClosest.hits.size());
    for (std::size_t index = 0; index != expectedClosest.hits.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest.hits[index]);
    }

    const VulkanWavefrontAnyHitKernelResult expectedAny = prepared.runTimedBasicAnyHitKernel(rays);
    const VulkanWavefrontAnyHitKernelResult actualAny =
      prepared.runTimedBasicAnyHitKernel(*rayBatch);
    ASSERT_EQ(expectedAny.records.size(), actualAny.records.size());
    for (std::size_t index = 0; index != expectedAny.records.size(); ++index) {
      EXPECT_EQ(expectedAny.records[index].rayIndex, actualAny.records[index].rayIndex);
      EXPECT_EQ(expectedAny.records[index].occluded, actualAny.records[index].occluded);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront backend is disabled";
#endif
  }

  TEST(VulkanWavefrontPreparedScene, CompactsPreparedRayBatchBeforeQueries) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Vulkan compute device is available";
    }
    if (!kernel.renderPathAvailable()) {
      GTEST_SKIP() << kernel.renderPathUnavailableReason();
    }

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 6), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers));

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 91),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 92),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 93, 0.0,
                                           2.5),
    };

    const VulkanWavefrontPreparedScene prepared(buffers);
    const std::shared_ptr<const VulkanWavefrontPreparedRayBatch> rayBatch =
      prepared.prepareRays(rays);
    ASSERT_NE(nullptr, rayBatch);
    EXPECT_THROW((void)prepared.compactRays(*rayBatch, {3u}), std::out_of_range);

    const VulkanWavefrontRayBatchCompactionResult timedCompaction =
      prepared.compactRaysTimed(*rayBatch, {2u, 0u});
    EXPECT_GE(timedCompaction.timing.uploadSeconds, 0.0);
    EXPECT_GE(timedCompaction.timing.kernelSeconds, 0.0);
    EXPECT_EQ(0.0, timedCompaction.timing.readbackSeconds);
    const std::shared_ptr<const VulkanWavefrontPreparedRayBatch> compacted = timedCompaction.rays;
    ASSERT_NE(nullptr, compacted);
    EXPECT_EQ(2u, compacted->rayCount());
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), compacted->packedRayBytes());

    const std::vector<GpuIntersectionRay> compactedRays{rays[2], rays[0]};
    const VulkanWavefrontClosestHitKernelResult expectedClosest =
      prepared.runTimedBasicClosestHitKernel(compactedRays);
    const VulkanWavefrontClosestHitKernelResult actualClosest =
      prepared.runTimedBasicClosestHitKernel(*compacted);

    ASSERT_EQ(expectedClosest.hits.size(), actualClosest.hits.size());
    for (std::size_t index = 0; index != expectedClosest.hits.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest.hits[index]);
    }

    const VulkanWavefrontAnyHitKernelResult expectedAny =
      prepared.runTimedBasicAnyHitKernel(compactedRays);
    const VulkanWavefrontAnyHitKernelResult actualAny =
      prepared.runTimedBasicAnyHitKernel(*compacted);
    ASSERT_EQ(expectedAny.records.size(), actualAny.records.size());
    for (std::size_t index = 0; index != expectedAny.records.size(); ++index) {
      EXPECT_EQ(expectedAny.records[index].rayIndex, actualAny.records[index].rayIndex);
      EXPECT_EQ(expectedAny.records[index].occluded, actualAny.records[index].occluded);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront backend is disabled";
#endif
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
    auto mesh = triangleMeshAt(4.0, 3.0);
    scene.add(std::make_shared<FlatMeshTriangle>(mesh.get(), 0, 1, 2));

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
    auto mesh = triangleMeshAt(4.0, 3.0);
    scene.add(std::make_shared<FlatMeshTriangle>(mesh.get(), 0, 1, 2));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.triangleClosestHitKernelEligible());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 7, 0.0,
                                           3.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 8, 0.0,
                                           4.0),
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

  TEST(MetalWavefrontPreparedScene, ReusesPreparedRayBatchAcrossClosestAndAnyHitQueries) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }
    if (!kernel.renderPathAvailable()) {
      GTEST_SKIP() << kernel.renderPathUnavailableReason();
    }

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 6), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.basicHitKernelEligible());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 91),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 92),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 93, 0.0,
                                           2.5),
    };

    const MetalWavefrontPreparedScene prepared(buffers);
    const std::shared_ptr<const MetalWavefrontPreparedRayBatch> rayBatch =
      prepared.prepareRays(rays);
    ASSERT_NE(nullptr, rayBatch);
    EXPECT_EQ(rays.size(), rayBatch->rayCount());
    EXPECT_EQ(rays.size() * sizeof(GpuIntersectionRay), rayBatch->packedRayBytes());

    const MetalWavefrontClosestHitKernelResult expectedClosest =
      prepared.runTimedBasicClosestHitKernel(rays);
    const MetalWavefrontClosestHitKernelResult actualClosest =
      prepared.runTimedBasicClosestHitKernel(*rayBatch);

    ASSERT_EQ(expectedClosest.hits.size(), actualClosest.hits.size());
    for (std::size_t index = 0; index != expectedClosest.hits.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest.hits[index]);
    }

    const MetalWavefrontAnyHitKernelResult expectedAny = prepared.runTimedBasicAnyHitKernel(rays);
    const MetalWavefrontAnyHitKernelResult actualAny =
      prepared.runTimedBasicAnyHitKernel(*rayBatch);
    ASSERT_EQ(expectedAny.records.size(), actualAny.records.size());
    for (std::size_t index = 0; index != expectedAny.records.size(); ++index) {
      EXPECT_EQ(expectedAny.records[index].rayIndex, actualAny.records[index].rayIndex);
      EXPECT_EQ(expectedAny.records[index].occluded, actualAny.records[index].occluded);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontPreparedScene, CompactsPreparedRayBatchBeforeQueries) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }
    if (!kernel.renderPathAvailable()) {
      GTEST_SKIP() << kernel.renderPathUnavailableReason();
    }

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 6), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.basicHitKernelEligible());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 91),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 92),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 93, 0.0,
                                           2.5),
    };

    const MetalWavefrontPreparedScene prepared(buffers);
    const std::shared_ptr<const MetalWavefrontPreparedRayBatch> rayBatch =
      prepared.prepareRays(rays);
    ASSERT_NE(nullptr, rayBatch);
    EXPECT_THROW((void)prepared.compactRays(*rayBatch, {3u}), std::out_of_range);

    const MetalWavefrontRayBatchCompactionResult timedCompaction =
      prepared.compactRaysTimed(*rayBatch, {2u, 0u});
    EXPECT_GE(timedCompaction.timing.uploadSeconds, 0.0);
    EXPECT_GE(timedCompaction.timing.kernelSeconds, 0.0);
    EXPECT_EQ(0.0, timedCompaction.timing.readbackSeconds);
    const std::shared_ptr<const MetalWavefrontPreparedRayBatch> compacted = timedCompaction.rays;
    ASSERT_NE(nullptr, compacted);
    EXPECT_EQ(2u, compacted->rayCount());
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), compacted->packedRayBytes());

    const std::vector<GpuIntersectionRay> compactedRays{rays[2], rays[0]};
    const MetalWavefrontClosestHitKernelResult expectedClosest =
      prepared.runTimedBasicClosestHitKernel(compactedRays);
    const MetalWavefrontClosestHitKernelResult actualClosest =
      prepared.runTimedBasicClosestHitKernel(*compacted);

    ASSERT_EQ(expectedClosest.hits.size(), actualClosest.hits.size());
    for (std::size_t index = 0; index != expectedClosest.hits.size(); ++index) {
      expectGpuHitRecordNear(actualClosest.hits[index], expectedClosest.hits[index]);
    }

    const MetalWavefrontAnyHitKernelResult expectedAny =
      prepared.runTimedBasicAnyHitKernel(compactedRays);
    const MetalWavefrontAnyHitKernelResult actualAny =
      prepared.runTimedBasicAnyHitKernel(*compacted);
    ASSERT_EQ(expectedAny.records.size(), actualAny.records.size());
    for (std::size_t index = 0; index != expectedAny.records.size(); ++index) {
      EXPECT_EQ(expectedAny.records[index].rayIndex, actualAny.records[index].rayIndex);
      EXPECT_EQ(expectedAny.records[index].occluded, actualAny.records[index].occluded);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontPreparedScene, SupportsConcurrentClosestAndAnyHitQueries) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }
    if (!kernel.renderPathAvailable()) {
      GTEST_SKIP() << kernel.renderPathUnavailableReason();
    }

    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 6), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.basicHitKernelEligible());

    const std::vector<GpuIntersectionRay> closestRays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 71),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 72),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 73, 0.0,
                                           2.5),
    };
    const std::vector<GpuIntersectionRay> anyRays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 81, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(4, 0, 0, 1), Vector3d(0, 0, 1)), 82, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 83, 0.0,
                                           2.5),
    };
    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, closestRays);
    const std::vector<GpuIntersectionOcclusionRecord> expectedAny =
      GpuIntersectionIntersector().intersectAny(buffers, anyRays);

    const MetalWavefrontPreparedScene prepared(buffers);
    auto closestFuture = std::async(
      std::launch::async, [&] { return prepared.runTimedBasicClosestHitKernel(closestRays); });
    auto anyFuture =
      std::async(std::launch::async, [&] { return prepared.runTimedBasicAnyHitKernel(anyRays); });

    const MetalWavefrontClosestHitKernelResult closest = closestFuture.get();
    const MetalWavefrontAnyHitKernelResult any = anyFuture.get();

    ASSERT_EQ(expectedClosest.size(), closest.hits.size());
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(closest.hits[index], expectedClosest[index]);
    }

    ASSERT_EQ(expectedAny.size(), any.records.size());
    for (std::size_t index = 0; index != expectedAny.size(); ++index) {
      EXPECT_EQ(expectedAny[index].occluded, any.records[index].occluded);
      EXPECT_EQ(expectedAny[index].rayIndex, any.records[index].rayIndex);
    }
#else
    GTEST_SKIP() << "Metal wavefront backend is disabled";
#endif
  }

  TEST(MetalWavefrontPreparedScene, ReusesPreparedOpenCylinderBuffersAcrossQueries) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    Scene scene;
    scene.add(std::make_shared<OpenCylinder>(1.0, 2.0));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_EQ(1u, buffers.openCylinders.size());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 41, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(1, 0, 0)), 42, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 2, -3, 1), Vector3d(0, 0, 1)), 43, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 44, 0.0,
                                           1.0),
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

  TEST(MetalWavefrontSmokeKernel, RunsOpenCylinderBasicHitKernelsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    Scene scene;
    scene.add(std::make_shared<OpenCylinder>(1.0, 2.0));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_EQ(1u, buffers.openCylinders.size());

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 41, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(1, 0, 0)), 42, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 2, -3, 1), Vector3d(0, 0, 1)), 43, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 44, 0.0,
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

  TEST(MetalWavefrontSmokeKernel, RunsStaticTransformBasicHitKernelsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    auto triangleMaterial =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::red()));
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    auto triangleInstance = std::make_shared<Instance>(triangle);
    triangleInstance->setMaterial(triangleMaterial);
    triangleInstance->setMatrix(Matrix4d::translate(0, 0, 2));

    auto sphereMaterial =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto sphereInstance = std::make_shared<Instance>(sphere);
    sphereInstance->setMaterial(sphereMaterial);
    sphereInstance->setMatrix(Matrix4d::translate(3, 0, 4));

    Scene scene;
    scene.add(triangleInstance);
    scene.add(sphereInstance);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    ASSERT_TRUE(buffers.packedClosestHitKernelEligible());
    ASSERT_TRUE(buffers.packedAnyHitKernelEligible());
    ASSERT_GT(buffers.transforms.size(), 2u);

    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -2, 1), Vector3d(0, 0, 1)), 91, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(3, 0, 0, 1), Vector3d(0, 0, 1)), 92, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(6, 0, 0, 1), Vector3d(0, 0, 1)), 93, 0.0,
                                           10.0),
    };
    const std::vector<GpuIntersectionHitRecord> expectedClosest =
      GpuIntersectionIntersector().intersectClosest(buffers, rays);
    ASSERT_TRUE(expectedClosest[0].hit);
    ASSERT_TRUE(expectedClosest[1].hit);
    ASSERT_LT(expectedClosest[0].object, compiled.objects().size());
    ASSERT_LT(expectedClosest[1].object, compiled.objects().size());
    ASSERT_LT(expectedClosest[0].material, compiled.materials().size());
    ASSERT_LT(expectedClosest[1].material, compiled.materials().size());
    EXPECT_EQ(triangleInstance.get(), compiled.objects()[expectedClosest[0].object]);
    EXPECT_EQ(sphereInstance.get(), compiled.objects()[expectedClosest[1].object]);
    EXPECT_EQ(triangleMaterial, compiled.materials()[expectedClosest[0].material]);
    EXPECT_EQ(sphereMaterial, compiled.materials()[expectedClosest[1].material]);

    const std::vector<GpuIntersectionHitRecord> actualClosest =
      kernel.runBasicClosestHitKernel(buffers, rays);
    ASSERT_EQ(expectedClosest.size(), actualClosest.size());
    for (std::size_t index = 0; index != expectedClosest.size(); ++index) {
      expectGpuHitRecordNear(actualClosest[index], expectedClosest[index]);
    }

    const std::vector<GpuIntersectionOcclusionRecord> expectedAny =
      GpuIntersectionIntersector().intersectAny(buffers, rays);
    const std::vector<GpuIntersectionOcclusionRecord> actualAny =
      kernel.runBasicAnyHitKernel(buffers, rays);
    ASSERT_EQ(expectedAny.size(), actualAny.size());
    for (std::size_t index = 0; index != expectedAny.size(); ++index) {
      EXPECT_EQ(expectedAny[index].occluded, actualAny[index].occluded);
      EXPECT_EQ(expectedAny[index].rayIndex, actualAny[index].rayIndex);
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
    scene.add(std::make_shared<Plane>(Vector3d(0, 0, 2), -18.0));
    scene.add(std::make_shared<Rectangle>(Vector3d(-1, -1, 5), Vector3d(2, 0, 0), Vector3d(0, 2, 0),
                                          Vector3d(0, 0, 2)));
    scene.add(std::make_shared<Disk>(Vector3d(3, 0, 4), Vector3d(0, 0, 2), 1.0));
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

  TEST(MetalWavefrontSmokeKernel, RunsTorusBasicHitKernelsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }

    Scene scene;
    scene.add(std::make_shared<Torus>(2.0, 0.5));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const std::vector<GpuIntersectionRay> rays{
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -4, 1), Vector3d(0, 0, 1)), 45, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0)), 46, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(2, 0, -2, 1), Vector3d(0, 0, 1)), 47, 0.0,
                                           10.0),
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, -4, 1), Vector3d(0, 0, 1)), 48, 0.0,
                                           1.0),
    };

    EXPECT_TRUE(buffers.basicHitKernelEligible());
    EXPECT_TRUE(buffers.packedClosestHitKernelEligible());
    EXPECT_TRUE(buffers.packedAnyHitKernelEligible());

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

  TEST(WavefrontIntersectionBackend, AutoChoiceCompilesSupportedSceneOnlyWhenPlatformAvailable) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 1000000;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::automatic().createBackendForScene(scene, context);

    EXPECT_STREQ("auto", backend->requestedName());
    if (!hostPlatformIntersectionDeviceAvailable() ||
        !hostPlatformIntersectionRenderPathAvailable()) {
      EXPECT_STREQ("cpu", backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("runtime_scene", backend->executionPath());
      EXPECT_NE(std::string::npos,
                std::string(backend->fallbackReason()).find("auto selected CPU"));
      const std::string reason = backend->fallbackReason();
      const bool disabled = reason.find("not enabled") != std::string::npos;
      const bool enabledWithoutClosestHitKernel =
        reason.find("no render-path closest-hit kernel") != std::string::npos;
      const bool enabledWithoutBasicHitKernel =
        reason.find("no render-path basic hit kernel") != std::string::npos ||
        reason.find("no render-path triangle/sphere hit kernel") != std::string::npos ||
        reason.find("no render-path exact-primitive hit kernel") != std::string::npos;
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
    if (std::string(backend->executionPath()) == backend->platformName()) {
      EXPECT_STREQ(backend->platformName(), backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
    } else {
      EXPECT_STREQ("cpu", backend->name());
      EXPECT_STREQ("fallback", backend->availability());
      EXPECT_STREQ("packed_cpu", backend->executionPath());
    }
  }

  TEST(WavefrontIntersectionBackend, AutoChoiceRejectsSmallWorkloadBeforeSceneCompile) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    WavefrontIntersectionBackendSelectionContext context;
    context.expectedRayCount = 63;
    context.minimumGpuRayCount = 64;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::automatic().createBackendForScene(scene, context);

    EXPECT_STREQ("auto", backend->requestedName());
    EXPECT_STREQ("cpu", backend->name());
    EXPECT_STREQ("available", backend->availability());
    EXPECT_STREQ("runtime_scene", backend->executionPath());
    const std::string reason = backend->fallbackReason();
    EXPECT_NE(std::string::npos, reason.find("expected ray count 63"));
    EXPECT_NE(std::string::npos, reason.find("fixed GPU threshold 64"));
    EXPECT_NE(std::string::npos, reason.find("before scene compilation"));
    EXPECT_EQ(nullptr, backend->compiledScene());
    EXPECT_FALSE(backend->compiledSceneDiagnostics().compiled);
    EXPECT_EQ(0u, backend->compiledSceneDiagnostics().uploadBytes);
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
    if (std::string(backend->name()) == "vulkan") {
      EXPECT_STREQ("gpu", backend->requestedName());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
      EXPECT_STREQ("vulkan", backend->executionPath());
      EXPECT_STREQ("vulkan", backend->closestHitExecutionPath());
      EXPECT_STREQ("vulkan", backend->anyHitExecutionPath());
    } else {
      expectUnavailablePlatformFallback(*backend, "Vulkan", "packed_cpu", "vulkan");
    }
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
    EXPECT_EQ(0u, diagnostics.openCylinders);
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

  TEST(WavefrontIntersectionBackend, GpuChoiceRetainsTorusBasicKernelScene) {
    Scene scene;
    scene.add(std::make_shared<Torus>(2.0, 0.5));

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_TRUE(backend->compiledScene()->fullySupported());
    EXPECT_EQ(1u, backend->compiledScene()->primitives().size());
    EXPECT_EQ(1u, backend->compiledScene()->tori().size());
    EXPECT_TRUE(backend->compiledScene()->unsupportedPrimitives().empty());

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(1u, diagnostics.primitives);
    EXPECT_EQ(1u, diagnostics.tori);
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
    auto instance = std::make_shared<Instance>(nullptr);
    instance->setName("render empty instance");
    Scene scene;
    scene.add(instance);

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    EXPECT_STREQ("gpu", backend->requestedName());
    EXPECT_STREQ("cpu", backend->name());
    EXPECT_STREQ("fallback", backend->availability());
    EXPECT_STREQ("runtime_scene", backend->executionPath());
    EXPECT_EQ(nullptr, backend->compiledScene());
    EXPECT_EQ(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_NE(std::string::npos, std::string(backend->fallbackReason()).find("unsupported"));
    EXPECT_NE(std::string::npos,
              std::string(backend->fallbackReason()).find("render empty instance"));

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(1u, diagnostics.primitives);
    EXPECT_EQ(1u, diagnostics.unsupportedPrimitives);
    ASSERT_EQ(1u, diagnostics.unsupportedReasons.size());
    EXPECT_EQ(1u, diagnostics.unsupportedReasons.at(
                    "empty instance is not supported by GPU intersection scene compiler"));
    EXPECT_EQ(0u, diagnostics.uploadBytes);
    EXPECT_FALSE(diagnostics.triangleClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.basicHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedAnyHitKernelEligible);
    EXPECT_EQ(0u, backend->estimatedClosestHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend->estimatedClosestHitReadbackBytes(4));
    EXPECT_EQ(0u, backend->estimatedAnyHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend->estimatedAnyHitReadbackBytes(4));
  }

  TEST(WavefrontIntersectionBackend, GpuChoiceRetainsTransparentMaterialScene) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    sphere->setName("glass sphere");
    sphere->setMaterial(std::make_shared<TransparentMaterial>());
    Scene scene;
    scene.add(sphere);

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    EXPECT_STREQ("gpu", backend->requestedName());
    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_TRUE(backend->compiledScene()->fullySupported());
    EXPECT_EQ(1u, backend->compiledScene()->primitives().size());
    EXPECT_EQ(1u, backend->compiledScene()->spheres().size());
    EXPECT_TRUE(backend->compiledScene()->unsupportedPrimitives().empty());

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(1u, diagnostics.primitives);
    EXPECT_EQ(1u, diagnostics.spheres);
    EXPECT_EQ(0u, diagnostics.unsupportedPrimitives);
    EXPECT_TRUE(diagnostics.unsupportedReasons.empty());
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

  TEST(WavefrontIntersectionBackend, GpuChoiceReportsEmptyInstanceUnsupportedReason) {
    auto instance = std::make_shared<Instance>(nullptr);
    instance->setName("empty asset instance");
    Scene scene;
    scene.add(instance);

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    EXPECT_STREQ("gpu", backend->requestedName());
    EXPECT_STREQ("cpu", backend->name());
    EXPECT_STREQ("fallback", backend->availability());
    EXPECT_STREQ("runtime_scene", backend->executionPath());
    EXPECT_EQ(nullptr, backend->compiledScene());
    EXPECT_EQ(nullptr, backend->gpuIntersectionSceneBuffers());
    const std::string reason = backend->fallbackReason();
    EXPECT_NE(std::string::npos, reason.find("empty asset instance"));
    EXPECT_NE(std::string::npos, reason.find("empty instance is not supported"));

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(1u, diagnostics.primitives);
    EXPECT_EQ(1u, diagnostics.unsupportedPrimitives);
    ASSERT_EQ(1u, diagnostics.unsupportedReasons.size());
    EXPECT_EQ(1u, diagnostics.unsupportedReasons.at(
                    "empty instance is not supported by GPU intersection scene compiler"));
    EXPECT_EQ(0u, diagnostics.uploadBytes);
    EXPECT_FALSE(diagnostics.triangleClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.basicHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedAnyHitKernelEligible);
    EXPECT_EQ(0u, backend->estimatedClosestHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend->estimatedClosestHitReadbackBytes(4));
    EXPECT_EQ(0u, backend->estimatedAnyHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend->estimatedAnyHitReadbackBytes(4));
  }

  TEST(WavefrontIntersectionBackend, GpuChoiceSummarizesUnsupportedReasonCounts) {
    auto firstInstance = std::make_shared<Instance>(nullptr);
    firstInstance->setName("first empty instance");
    auto secondInstance = std::make_shared<Instance>(nullptr);
    secondInstance->setName("second empty instance");
    Scene scene;
    scene.add(firstInstance);
    scene.add(secondInstance);

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    EXPECT_STREQ("gpu", backend->requestedName());
    EXPECT_STREQ("cpu", backend->name());
    EXPECT_STREQ("fallback", backend->availability());
    EXPECT_EQ(
      "GPU intersection scene unsupported: first empty instance: empty instance is not supported "
      "by GPU "
      "intersection scene compiler (2 unsupported leaves; 2x empty instance is not supported by "
      "GPU intersection scene compiler)",
      std::string(backend->fallbackReason()));

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    ASSERT_EQ(1u, diagnostics.unsupportedReasons.size());
    EXPECT_EQ(2u, diagnostics.unsupportedReasons.at(
                    "empty instance is not supported by GPU intersection scene compiler"));
  }

  TEST(WavefrontIntersectionBackend, GpuChoiceDoesNotEstimateTransferForIneligiblePreparedScene) {
    Scene scene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_TRUE(backend->compiledScene()->fullySupported());
    EXPECT_TRUE(backend->compiledScene()->primitives().empty());
    EXPECT_FALSE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());
    EXPECT_FALSE(backend->gpuIntersectionSceneBuffers()->packedAnyHitKernelEligible());

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(0u, diagnostics.primitives);
    EXPECT_EQ(0u, diagnostics.unsupportedPrimitives);
    EXPECT_EQ(0u, diagnostics.uploadBytes);
    EXPECT_FALSE(diagnostics.packedClosestHitKernelEligible);
    EXPECT_FALSE(diagnostics.packedAnyHitKernelEligible);
    EXPECT_EQ(0u, backend->estimatedClosestHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend->estimatedClosestHitReadbackBytes(4));
    EXPECT_EQ(0u, backend->estimatedAnyHitRayUploadBytes(4));
    EXPECT_EQ(0u, backend->estimatedAnyHitReadbackBytes(4));
  }

  TEST(WavefrontIntersectionBackend, GpuChoiceReportsOpenCylinderSceneDiagnostics) {
    Scene scene;
    scene.add(std::make_shared<OpenCylinder>(1.0, 2.0));

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_EQ(1u, backend->compiledScene()->openCylinders().size());
    EXPECT_EQ(1u, backend->gpuIntersectionSceneBuffers()->openCylinders.size());

    const WavefrontIntersectionSceneDiagnostics diagnostics = backend->compiledSceneDiagnostics();
    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(1u, diagnostics.primitives);
    EXPECT_EQ(0u, diagnostics.triangles);
    EXPECT_EQ(0u, diagnostics.spheres);
    EXPECT_EQ(1u, diagnostics.openCylinders);
    EXPECT_EQ(0u, diagnostics.unsupportedPrimitives);
    EXPECT_TRUE(diagnostics.basicHitKernelEligible);
    EXPECT_TRUE(diagnostics.packedClosestHitKernelEligible);
    EXPECT_TRUE(diagnostics.packedAnyHitKernelEligible);
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
    const std::string closestPath = backend->closestHitExecutionPath();
    if (closestPath == "metal" || closestPath == "vulkan") {
      EXPECT_STREQ(closestPath.c_str(), backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
      EXPECT_STREQ(closestPath.c_str(), backend->executionPath());
      EXPECT_STREQ(closestPath.c_str(), backend->anyHitExecutionPath());
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

  TEST(WavefrontIntersectionBackend, PreparedGpuClosestHitFrontierOwnsPackedRayPayload) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    ASSERT_TRUE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());

    State hitState;
    State missState;
    std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &hitState},
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 3, -4), Vector3d(0, 0, 1)), &missState}};

    const std::unique_ptr<WavefrontClosestHitFrontier> frontier =
      backend->createClosestHitFrontier(std::move(queries));

    ASSERT_NE(nullptr, frontier);
    EXPECT_EQ(2u, frontier->rayCount());
    const std::string backendName = backend->name();
    const char* expectedResidency = backendName == "metal"    ? "metal_shared"
                                    : backendName == "vulkan" ? "vulkan_host_coherent"
                                                              : "packed_host";
    EXPECT_STREQ(expectedResidency, frontier->residency());
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), frontier->packedRayBytes());
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), frontier->hostPackedRayBytes());
    EXPECT_EQ(0u, frontier->hostQueryBytes());
    EXPECT_EQ(2u * sizeof(State*), frontier->stateHandleBytes());

    WavefrontIntersectionQueryTiming timing;
    const std::vector<WavefrontClosestHitResult> hits =
      backend->intersectClosestFrontier(emptyScene, *frontier, &timing);

    ASSERT_EQ(2u, hits.size());
    EXPECT_TRUE(hits[0].hit());
    EXPECT_EQ(sphere.get(), hits[0].primitive);
    EXPECT_NEAR(3.0, hits[0].hitPoint.distance(), 1e-5);
    EXPECT_FALSE(hits[1].hit());
    EXPECT_EQ(1, hitState.intersectionHits);
    EXPECT_EQ(0, hitState.intersectionMisses);
    EXPECT_EQ(0, missState.intersectionHits);
    EXPECT_EQ(1, missState.intersectionMisses);
    EXPECT_TRUE(timing.executionPath == "packed_cpu" || timing.executionPath == "metal" ||
                timing.executionPath == "vulkan")
      << timing.executionPath;
    EXPECT_GE(timing.uploadSeconds, 0.0);
  }

  TEST(WavefrontIntersectionBackend, PreparedClosestHitFrontierCanOwnPackedExecution) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);
    PreparedFrontierTestBackend backend(sourceScene);

    State hitState;
    State missState;
    std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &hitState},
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 3, -4), Vector3d(0, 0, 1)), &missState}};

    const GpuIntersectionSceneBuffers buffers =
      GpuIntersectionScenePacker().packScene(*backend.compiledScene());
    const std::vector<GpuIntersectionRay> packedRays{
      GpuIntersectionScenePacker().packRay(queries[0].ray, 0),
      GpuIntersectionScenePacker().packRay(queries[1].ray, 1),
    };
    std::vector<GpuIntersectionHitRecord> records =
      GpuIntersectionIntersector().intersectClosest(buffers, packedRays);
    const SyntheticPackedClosestHitFrontier frontier(std::move(queries), std::move(records));

    WavefrontIntersectionQueryTiming timing;
    const std::vector<WavefrontClosestHitResult> hits =
      backend.intersectPreparedClosestFrontier(frontier, &timing);

    ASSERT_EQ(2u, hits.size());
    EXPECT_TRUE(hits[0].hit());
    EXPECT_EQ(sphere.get(), hits[0].primitive);
    EXPECT_NEAR(3.0, hits[0].hitPoint.distance(), 1e-5);
    EXPECT_FALSE(hits[1].hit());
    EXPECT_EQ(1, hitState.intersectionHits);
    EXPECT_EQ(0, hitState.intersectionMisses);
    EXPECT_EQ(0, missState.intersectionHits);
    EXPECT_EQ(1, missState.intersectionMisses);
    EXPECT_EQ("synthetic_closest_frontier", timing.executionPath);
  }

  TEST(WavefrontIntersectionBackend, PreparedClosestHitFrontierRejectsMismatchedPackedRecords) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    PreparedFrontierTestBackend backend(sourceScene);

    State firstState;
    State secondState;
    std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &firstState},
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 3, -4), Vector3d(0, 0, 1)), &secondState}};
    std::vector<GpuIntersectionHitRecord> records(1);
    const SyntheticPackedClosestHitFrontier frontier(std::move(queries), std::move(records));

    EXPECT_THROW(
      {
        const std::vector<WavefrontClosestHitResult> hits =
          backend.intersectPreparedClosestFrontier(frontier);
        (void)hits;
      },
      std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, PreparedClosestHitBatchRejectsMismatchedPackedRecords) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    MalformedPackedRecordBackend backend(sourceScene, PackedRecordFailure::Short);

    State firstState;
    State secondState;
    const std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &firstState},
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 3, -4), Vector3d(0, 0, 1)), &secondState}};

    EXPECT_THROW(
      {
        const std::vector<WavefrontClosestHitResult> hits =
          backend.intersectPreparedClosestBatch(queries);
        (void)hits;
      },
      std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, PreparedClosestHitFrontierRejectsDuplicatePackedRayIndices) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    PreparedFrontierTestBackend backend(sourceScene);

    State firstState;
    State secondState;
    std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &firstState},
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 3, -4), Vector3d(0, 0, 1)), &secondState}};
    std::vector<GpuIntersectionHitRecord> records(2);
    records[0].rayIndex = 0;
    records[1].rayIndex = 0;
    const SyntheticPackedClosestHitFrontier frontier(std::move(queries), std::move(records));

    EXPECT_THROW(
      {
        const std::vector<WavefrontClosestHitResult> hits =
          backend.intersectPreparedClosestFrontier(frontier);
        (void)hits;
      },
      std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, PreparedClosestHitBatchRejectsDuplicatePackedRayIndices) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    MalformedPackedRecordBackend backend(sourceScene, PackedRecordFailure::DuplicateRayIndex);

    State firstState;
    State secondState;
    const std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &firstState},
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 3, -4), Vector3d(0, 0, 1)), &secondState}};

    EXPECT_THROW(
      {
        const std::vector<WavefrontClosestHitResult> hits =
          backend.intersectPreparedClosestBatch(queries);
        (void)hits;
      },
      std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuClosestHitBatchPreservesInheritedMaterial) {
    auto material =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto cylinder = std::make_shared<ClosedSolidUnion>();
    cylinder->setMaterial(material);
    cylinder->add(std::make_shared<OpenCylinder>(1.0, 2.0));
    cylinder->add(std::make_shared<Disk>(Vector3d(0, -1, 0), Vector3d(0, -1, 0), 1.0));
    cylinder->add(std::make_shared<Disk>(Vector3d(0, 1, 0), Vector3d(0, 1, 0), 1.0));
    Scene sourceScene;
    sourceScene.add(cylinder);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    ASSERT_TRUE(backend->prefersClosestHitBatch(2));

    State sideState;
    State missState;
    const std::vector<WavefrontClosestHitQuery> queries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &sideState},
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 3, -4), Vector3d(0, 0, 1)), &missState}};

    const std::vector<WavefrontClosestHitResult> hits =
      backend->intersectClosestBatch(emptyScene, queries);

    ASSERT_EQ(2u, hits.size());
    ASSERT_TRUE(hits[0].hit());
    EXPECT_NE(nullptr, hits[0].primitive);
    EXPECT_EQ(material, hits[0].material);
    EXPECT_FALSE(hits[1].hit());
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuClosestHitResultPreservesInheritedMaterial) {
    auto material =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto cylinder = std::make_shared<ClosedSolidUnion>();
    cylinder->setMaterial(material);
    cylinder->add(std::make_shared<OpenCylinder>(1.0, 2.0));
    cylinder->add(std::make_shared<Disk>(Vector3d(0, -1, 0), Vector3d(0, -1, 0), 1.0));
    cylinder->add(std::make_shared<Disk>(Vector3d(0, 1, 0), Vector3d(0, 1, 0), 1.0));
    Scene sourceScene;
    sourceScene.add(cylinder);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    ASSERT_FALSE(backend->prefersClosestHitBatch(1));

    State state;
    const WavefrontClosestHitResult hit = backend->intersectClosestResult(
      emptyScene, Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), state);

    ASSERT_TRUE(hit.hit());
    EXPECT_EQ(cylinder.get(), hit.primitive);
    EXPECT_EQ(material, hit.material);
    EXPECT_EQ(1, state.intersectionHits);
    EXPECT_EQ(0, state.intersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedPackedQueriesReportPreparedTimingPath) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    auto instance = std::make_shared<Instance>(triangle);
    instance->setMatrix(Matrix4d::translate(0, 0, 1));
    Scene sourceScene;
    sourceScene.add(instance);
    Scene emptyScene;

    auto compiled = std::make_shared<const CompiledIntersectionScene>(
      IntersectionSceneCompiler().compile(sourceScene));
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      VulkanWavefrontIntersectionBackend::createPrepared(compiled);
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    EXPECT_TRUE(VulkanWavefrontIntersectionBackend::supportsPackedScene(
      *backend->gpuIntersectionSceneBuffers()));
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());
    EXPECT_TRUE(backend->gpuIntersectionSceneBuffers()->packedAnyHitKernelEligible());

    State closestState;
    const std::vector<WavefrontClosestHitQuery> closestQueries{
      WavefrontClosestHitQuery{Rayd(Vector3d(0, 0, -4), Vector3d(0, 0, 1)), &closestState}};
    WavefrontIntersectionQueryTiming closestTiming;
    const std::vector<WavefrontClosestHitResult> closestHits =
      backend->intersectClosestBatch(emptyScene, closestQueries, &closestTiming);

    ASSERT_EQ(1u, closestHits.size());
    EXPECT_TRUE(closestHits.front().hit());
    EXPECT_TRUE(closestTiming.executionPath == "packed_cpu" ||
                closestTiming.executionPath == "vulkan")
      << closestTiming.executionPath;

    State anyState;
    const std::vector<WavefrontAnyHitQuery> anyQueries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 5.0, &anyState}};
    WavefrontIntersectionQueryTiming anyTiming;
    const WavefrontOcclusionFlags anyHits =
      backend->intersectAnyBatch(emptyScene, anyQueries, &anyTiming);

    ASSERT_EQ(1u, anyHits.size());
    EXPECT_TRUE(anyHits.front());
    EXPECT_TRUE(anyTiming.executionPath == "packed_cpu" || anyTiming.executionPath == "vulkan")
      << anyTiming.executionPath;
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuClosestHitUsesRetainedPackedStaticTransformScene) {
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
    const std::string closestPath = backend->closestHitExecutionPath();
    if (closestPath == "metal" || closestPath == "vulkan") {
      EXPECT_STREQ(closestPath.c_str(), backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
      EXPECT_STREQ(closestPath.c_str(), backend->executionPath());
      EXPECT_STREQ(closestPath.c_str(), backend->anyHitExecutionPath());
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
    const std::string closestPath = backend->closestHitExecutionPath();
    if (closestPath == "metal" || closestPath == "vulkan") {
      EXPECT_STREQ(closestPath.c_str(), backend->name());
      EXPECT_STREQ("available", backend->availability());
      EXPECT_STREQ("", backend->fallbackReason());
      EXPECT_STREQ(closestPath.c_str(), backend->executionPath());
      EXPECT_STREQ(closestPath.c_str(), backend->anyHitExecutionPath());
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

  TEST(WavefrontIntersectionBackend, PreparedGpuClosestHitFallsBackToPrimitiveMaterial) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    Scene sourceScene;
    sourceScene.add(triangle);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    ASSERT_TRUE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());

    auto material =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::blue()));
    triangle->setMaterial(material);

    State state;
    const WavefrontClosestHitResult hit = backend->intersectClosestResult(
      emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), state);

    ASSERT_TRUE(hit.hit());
    EXPECT_EQ(triangle.get(), hit.primitive);
    EXPECT_EQ(material, hit.material);
    EXPECT_EQ(1, state.intersectionHits);
    EXPECT_EQ(0, state.intersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuClosestHitPreservesInstanceMaterialOwner) {
    auto material =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto instance = std::make_shared<Instance>(sphere);
    instance->setMaterial(material);
    instance->setMatrix(Matrix4d::translate(0, 0, 2));
    Scene sourceScene;
    sourceScene.add(instance);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    ASSERT_TRUE(backend->gpuIntersectionSceneBuffers()->packedClosestHitKernelEligible());

    State state;
    const WavefrontClosestHitResult hit = backend->intersectClosestResult(
      emptyScene, Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), state);

    ASSERT_TRUE(hit.hit());
    EXPECT_EQ(instance.get(), hit.primitive);
    EXPECT_EQ(material, hit.material);
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
    const std::string anyPath = backend->anyHitExecutionPath();
    if (anyPath == "metal" || anyPath == "vulkan") {
      EXPECT_STREQ(anyPath.c_str(), backend->name());
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
    const std::string anyPath = backend->anyHitExecutionPath();
    if (anyPath == "metal" || anyPath == "vulkan") {
      EXPECT_STREQ(anyPath.c_str(), backend->name());
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
    const WavefrontOcclusionFlags occluded =
      backend->intersectAnyBatch(emptyScene, queries, &timing);

    ASSERT_EQ(2u, occluded.size());
    EXPECT_TRUE(occluded[0]);
    EXPECT_FALSE(occluded[1]);
    EXPECT_EQ(1, batchHitState.shadowIntersectionHits);
    EXPECT_EQ(0, batchHitState.shadowIntersectionMisses);
    EXPECT_EQ(0, batchMissState.shadowIntersectionHits);
    EXPECT_EQ(1, batchMissState.shadowIntersectionMisses);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuAnyHitFrontierOwnsPackedRayPayload) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);
    Scene emptyScene;

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    ASSERT_TRUE(backend->gpuIntersectionSceneBuffers()->packedAnyHitKernelEligible());

    State hitState;
    State missState;
    std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 3.0, &hitState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &missState}};

    const std::unique_ptr<WavefrontAnyHitFrontier> frontier =
      backend->createAnyHitFrontier(std::move(queries));

    ASSERT_NE(nullptr, frontier);
    EXPECT_EQ(2u, frontier->rayCount());
    const std::string backendName = backend->name();
    const char* expectedResidency = backendName == "metal"    ? "metal_shared"
                                    : backendName == "vulkan" ? "vulkan_host_coherent"
                                                              : "packed_host";
    EXPECT_STREQ(expectedResidency, frontier->residency());
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), frontier->packedRayBytes());
    EXPECT_EQ(2u * sizeof(GpuIntersectionRay), frontier->hostPackedRayBytes());
    EXPECT_EQ(0u, frontier->hostQueryBytes());
    EXPECT_EQ(2u * sizeof(State*), frontier->stateHandleBytes());

    WavefrontIntersectionQueryTiming timing;
    const WavefrontOcclusionFlags occluded =
      backend->intersectAnyFrontier(emptyScene, *frontier, &timing);

    ASSERT_EQ(2u, occluded.size());
    EXPECT_TRUE(occluded[0]);
    EXPECT_FALSE(occluded[1]);
    EXPECT_EQ(1, hitState.shadowIntersectionHits);
    EXPECT_EQ(0, hitState.shadowIntersectionMisses);
    EXPECT_EQ(0, missState.shadowIntersectionHits);
    EXPECT_EQ(1, missState.shadowIntersectionMisses);
    EXPECT_TRUE(timing.executionPath == "packed_cpu" || timing.executionPath == "metal" ||
                timing.executionPath == "vulkan")
      << timing.executionPath;
    EXPECT_GE(timing.uploadSeconds, 0.0);
  }

  TEST(WavefrontIntersectionBackend, PreparedGpuResidentFrontiersDoNotImplyResidentDirectLight) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    Scene sourceScene;
    sourceScene.add(sphere);

    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(sourceScene);

    ASSERT_NE(nullptr, backend->compiledScene());
    ASSERT_NE(nullptr, backend->gpuIntersectionSceneBuffers());
    const std::string backendName = backend->name();
    EXPECT_EQ(backendName == "metal" || backendName == "vulkan",
              backend->supportsResidentFrontiers());
    EXPECT_EQ(backendName == "metal" || backendName == "vulkan",
              backend->supportsPreparedRayBatchCompaction());
    EXPECT_FALSE(backend->supportsGpuFrontierCompaction());
    EXPECT_STREQ((backendName == "metal" || backendName == "vulkan")
                   ? "scheduler active path state is host-owned"
                   : "backend does not support prepared ray-batch compaction",
                 backend->gpuFrontierCompactionUnavailableReason());
    EXPECT_FALSE(backend->supportsResidentDirectLightBatches());
    EXPECT_STREQ((backendName == "metal" || backendName == "vulkan")
                   ? "shading creates direct-light rays on the host"
                   : "backend does not support resident frontiers",
                 backend->residentDirectLightBatchesUnavailableReason());
  }

  TEST(WavefrontIntersectionBackend, PreparedAnyHitFrontierCanOwnPackedExecution) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    PreparedFrontierTestBackend backend(sourceScene);

    State hitState;
    State missState;
    std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 3.0, &hitState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &missState}};

    const GpuIntersectionSceneBuffers buffers =
      GpuIntersectionScenePacker().packScene(*backend.compiledScene());
    const std::vector<GpuIntersectionRay> packedRays{
      GpuIntersectionScenePacker().packRay(queries[0].ray, 0, Ray<float>::epsilon,
                                           queries[0].maxDistance),
      GpuIntersectionScenePacker().packRay(queries[1].ray, 1, Ray<float>::epsilon,
                                           queries[1].maxDistance),
    };
    std::vector<GpuIntersectionOcclusionRecord> records =
      GpuIntersectionIntersector().intersectAny(buffers, packedRays);
    const SyntheticPackedAnyHitFrontier frontier(std::move(queries), std::move(records));

    WavefrontIntersectionQueryTiming timing;
    const WavefrontOcclusionFlags occluded =
      backend.intersectPreparedAnyFrontier(frontier, &timing);

    ASSERT_EQ(2u, occluded.size());
    EXPECT_TRUE(occluded[0]);
    EXPECT_FALSE(occluded[1]);
    EXPECT_EQ(1, hitState.shadowIntersectionHits);
    EXPECT_EQ(0, hitState.shadowIntersectionMisses);
    EXPECT_EQ(0, missState.shadowIntersectionHits);
    EXPECT_EQ(1, missState.shadowIntersectionMisses);
    EXPECT_EQ("synthetic_any_frontier", timing.executionPath);
  }

  TEST(WavefrontIntersectionBackend, PreparedAnyHitFrontierRejectsMismatchedPackedRecords) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    PreparedFrontierTestBackend backend(sourceScene);

    State firstState;
    State secondState;
    std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 3.0, &firstState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &secondState}};
    std::vector<GpuIntersectionOcclusionRecord> records(1);
    const SyntheticPackedAnyHitFrontier frontier(std::move(queries), std::move(records));

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = backend.intersectPreparedAnyFrontier(frontier);
        (void)occluded;
      },
      std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, PreparedAnyHitBatchRejectsMismatchedPackedRecords) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    MalformedPackedRecordBackend backend(sourceScene, PackedRecordFailure::Short);

    State firstState;
    State secondState;
    const std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 3.0, &firstState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &secondState}};

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = backend.intersectPreparedAnyBatch(queries);
        (void)occluded;
      },
      std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, PreparedAnyHitFrontierRejectsDuplicatePackedRayIndices) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    PreparedFrontierTestBackend backend(sourceScene);

    State firstState;
    State secondState;
    std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 3.0, &firstState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &secondState}};
    std::vector<GpuIntersectionOcclusionRecord> records(2);
    records[0].rayIndex = 0;
    records[1].rayIndex = 0;
    const SyntheticPackedAnyHitFrontier frontier(std::move(queries), std::move(records));

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = backend.intersectPreparedAnyFrontier(frontier);
        (void)occluded;
      },
      std::logic_error);
  }

  TEST(WavefrontIntersectionBackend, PreparedAnyHitBatchRejectsDuplicatePackedRayIndices) {
    Scene sourceScene;
    sourceScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    MalformedPackedRecordBackend backend(sourceScene, PackedRecordFailure::DuplicateRayIndex);

    State firstState;
    State secondState;
    const std::vector<WavefrontAnyHitQuery> queries{
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 3.0, &firstState},
      WavefrontAnyHitQuery{Rayd(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1)), 1.0, &secondState}};

    EXPECT_THROW(
      {
        const WavefrontOcclusionFlags occluded = backend.intersectPreparedAnyBatch(queries);
        (void)occluded;
      },
      std::logic_error);
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
