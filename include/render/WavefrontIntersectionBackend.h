#pragma once

#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "render/WavefrontIntersectionQueryTiming.h"
#include "render/primitives/Primitive.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace render {
  class CompiledIntersectionScene;
  class Material;
  struct GpuIntersectionHitRecord;
  struct GpuIntersectionOcclusionRecord;
  struct GpuIntersectionPrimitiveRecord;
  struct GpuIntersectionRay;
  struct GpuIntersectionSceneBuffers;
  class MetalWavefrontPreparedScene;
  class Scene;
  class State;
  class VulkanWavefrontPreparedScene;
  class WavefrontIntersectionBackend;

  struct WavefrontIntersectionSceneDiagnostics {
    bool compiled{false};
    std::uint64_t bvhNodes{0};
    std::uint64_t primitives{0};
    std::uint64_t triangles{0};
    std::uint64_t spheres{0};
    std::uint64_t planes{0};
    std::uint64_t rectangles{0};
    std::uint64_t disks{0};
    std::uint64_t openCylinders{0};
    std::uint64_t tori{0};
    std::uint64_t transforms{0};
    std::uint64_t unsupportedPrimitives{0};
    std::map<std::string, std::uint64_t> unsupportedReasons;
    std::uint64_t uploadBytes{0};
    bool triangleClosestHitKernelEligible{false};
    bool basicHitKernelEligible{false};
    bool packedClosestHitKernelEligible{false};
    bool packedAnyHitKernelEligible{false};

    [[nodiscard]] static WavefrontIntersectionSceneDiagnostics
    fromCompiledScene(const CompiledIntersectionScene& scene);
    [[nodiscard]] static WavefrontIntersectionSceneDiagnostics
    fromCompiledSceneAndUploadBuffers(const CompiledIntersectionScene& scene,
                                      const GpuIntersectionSceneBuffers& buffers);
  };

  struct WavefrontIntersectionBackendSelectionContext {
    std::uint64_t expectedRayCount{0};
    std::uint64_t expectedClosestHitRayCount{0};
    std::uint64_t expectedAnyHitRayCount{0};
    std::uint64_t minimumGpuRayCount{65536};
    std::uint64_t minimumGpuRaysPerSceneUploadKiB{64};

    [[nodiscard]] static WavefrontIntersectionBackendSelectionContext
    fromExpectedQueryFamilies(std::uint64_t expectedClosestHitRays,
                              std::uint64_t expectedAnyHitRays);
    void setExpectedQueryFamilies(std::uint64_t expectedClosestHitRays,
                                  std::uint64_t expectedAnyHitRays);
    [[nodiscard]] bool hasExpectedQueryFamilies() const;
    [[nodiscard]] std::uint64_t effectiveExpectedRayCount() const;
    [[nodiscard]] static std::uint64_t
    saturatedExpectedRayCount(std::uint64_t expectedClosestHitRays,
                              std::uint64_t expectedAnyHitRays);
  };

  struct WavefrontAnyHitQuery {
    Rayd ray;
    double maxDistance{0.0};
    State* state{nullptr};
  };

  struct WavefrontClosestHitQuery {
    Rayd ray;
    State* state{nullptr};
  };

  struct WavefrontClosestHitResult {
    const Primitive* primitive{nullptr};
    std::shared_ptr<Material> material;
    HitPoint hitPoint;

    [[nodiscard]] bool hit() const {
      return primitive != nullptr && !hitPoint.isUndefined();
    }
  };

  struct WavefrontIntersectionBackendAutoSelectionDecision {
    bool useGpu{false};
    std::uint64_t minimumExpectedRayCount{0};
    std::string reason;
    std::uint64_t estimatedQueryTransferBytes{0};
  };

  class WavefrontIntersectionBackendAutoSelectionPolicy {
  public:
    [[nodiscard]] WavefrontIntersectionBackendAutoSelectionDecision
    decide(bool platformGpuDeviceAvailable, bool platformGpuRenderPathAvailable,
           const WavefrontIntersectionSceneDiagnostics& diagnostics,
           const WavefrontIntersectionBackendSelectionContext& context) const;
    [[nodiscard]] std::optional<WavefrontIntersectionBackendAutoSelectionDecision>
    decideBeforeSceneCompile(const WavefrontIntersectionBackendSelectionContext& context) const;
    [[nodiscard]] std::uint64_t
    minimumExpectedRayCount(const WavefrontIntersectionSceneDiagnostics& diagnostics,
                            const WavefrontIntersectionBackendSelectionContext& context) const;
    [[nodiscard]] std::uint64_t
    estimatedQueryTransferBytes(const WavefrontIntersectionSceneDiagnostics& diagnostics,
                                const WavefrontIntersectionBackendSelectionContext& context) const;

  private:
    [[nodiscard]] bool
    sceneCanUseGpu(const WavefrontIntersectionSceneDiagnostics& diagnostics) const;
    [[nodiscard]] bool
    expectedRayCountJustifiesGpu(const WavefrontIntersectionSceneDiagnostics& diagnostics,
                                 const WavefrontIntersectionBackendSelectionContext& context) const;
    [[nodiscard]] std::uint64_t
    sceneUploadKiB(const WavefrontIntersectionSceneDiagnostics& diagnostics) const;
    [[nodiscard]] std::uint64_t saturatingProduct(std::uint64_t lhs, std::uint64_t rhs) const;
    [[nodiscard]] std::uint64_t saturatingSum(std::uint64_t lhs, std::uint64_t rhs) const;
  };

  /**
    * User/intent-level choice for wavefront ray-scene intersection work.
    *
    * `cpu` always uses the canonical runtime Scene traversal. `gpu` requests
    * the host platform backend directly and falls back visibly when the platform
    * backend is unavailable or the scene cannot use the packed kernel ABI.
    * `auto` uses the same platform path only when availability, scene support,
    * and expected ray count justify the transfer cost.
    */
  class WavefrontIntersectionBackendChoice {
  public:
    enum class Kind { Auto, CPU, GPU };

    WavefrontIntersectionBackendChoice();
    explicit WavefrontIntersectionBackendChoice(Kind kind);

    static WavefrontIntersectionBackendChoice automatic();
    static WavefrontIntersectionBackendChoice cpu();
    static WavefrontIntersectionBackendChoice gpu();
    static WavefrontIntersectionBackendChoice fromString(std::string value);

    Kind kind() const;
    const char* id() const;
    const WavefrontIntersectionBackend& resolvedBackend() const;
    std::shared_ptr<const WavefrontIntersectionBackend>
    createBackendForScene(const Scene& scene) const;
    std::shared_ptr<const WavefrontIntersectionBackend>
    createBackendForScene(const Scene& scene,
                          const WavefrontIntersectionBackendSelectionContext& context) const;

    bool operator==(const WavefrontIntersectionBackendChoice& other) const;
    bool operator!=(const WavefrontIntersectionBackendChoice& other) const;

  private:
    class SelectionStrategy;
    class AutoSelectionStrategy;
    class CpuSelectionStrategy;
    class GpuSelectionStrategy;

    Kind m_kind;

    [[nodiscard]] const SelectionStrategy& selectionStrategy() const;
    [[nodiscard]] std::shared_ptr<const WavefrontIntersectionBackend> makeDelegatingBackend(
      std::string requestedName, std::string availability, std::string fallbackReason,
      WavefrontIntersectionSceneDiagnostics diagnostics = {}, std::string platformName = {}) const;
    [[nodiscard]] std::shared_ptr<const WavefrontIntersectionBackend>
    staticBackend(const WavefrontIntersectionBackend& backend) const;
    [[nodiscard]] std::string
    gpuSceneUnsupportedReason(const CompiledIntersectionScene& scene) const;
    [[nodiscard]] const WavefrontIntersectionBackend& automaticCpuBackend() const;
    [[nodiscard]] const WavefrontIntersectionBackend& gpuUnavailableBackend() const;
    [[nodiscard]] bool hostPlatformGpuBackendAvailable() const;
    [[nodiscard]] bool hostPlatformGpuRenderPathAvailable() const;
    [[nodiscard]] std::shared_ptr<const WavefrontIntersectionBackend>
    createPreparedGpuBackend(std::shared_ptr<const CompiledIntersectionScene> scene,
                             std::string requestedName) const;

    [[nodiscard]] static std::string normalized(std::string value);
  };

  /**
    * @brief Ray-scene intersection boundary used by wavefront batch integrators.
    *
    * Wavefront scheduling and shading stay in the integrators. This backend
    * answers the narrower question those schedulers need before each shading
    * depth: what did this ray, or this packet of rays, hit?
    */
  class WavefrontIntersectionBackend {
  public:
    virtual ~WavefrontIntersectionBackend() = default;

    virtual const char* name() const = 0;
    virtual const char* platformName() const;
    virtual const char* requestedName() const;
    virtual const char* availability() const;
    virtual const char* fallbackReason() const;
    virtual const char* executionPath() const;
    virtual const char* closestHitExecutionPath() const;
    virtual const char* anyHitExecutionPath() const;
    virtual bool platformGpuDeviceAvailable() const;
    virtual bool platformGpuRenderPathAvailable() const;
    virtual const CompiledIntersectionScene* compiledScene() const;
    virtual const GpuIntersectionSceneBuffers* gpuIntersectionSceneBuffers() const;
    virtual WavefrontIntersectionSceneDiagnostics compiledSceneDiagnostics() const;
    virtual std::uint64_t estimatedClosestHitRayUploadBytes(std::uint64_t submittedRays) const;
    virtual std::uint64_t estimatedClosestHitReadbackBytes(std::uint64_t submittedRays) const;
    virtual std::uint64_t estimatedAnyHitRayUploadBytes(std::uint64_t submittedRays) const;
    virtual std::uint64_t estimatedAnyHitReadbackBytes(std::uint64_t submittedRays) const;
    virtual bool prefersClosestHitBatch(std::uint64_t submittedRays) const;
    virtual bool prefersAnyHitBatch(std::uint64_t submittedRays) const;
    virtual bool supportsResidentFrontiers() const;
    virtual bool supportsGpuFrontierCompaction() const;
    virtual bool supportsResidentDirectLightBatches() const;

    virtual WavefrontClosestHitResult
    intersectClosestResult(const Scene& scene, const Rayd& ray, State& state,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const;
    virtual const Primitive*
    intersectClosest(const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints, State& state,
                     WavefrontIntersectionQueryTiming* timing = nullptr) const = 0;
    virtual std::vector<WavefrontClosestHitResult>
    intersectClosestBatch(const Scene& scene, const std::vector<WavefrontClosestHitQuery>& queries,
                          WavefrontIntersectionQueryTiming* timing = nullptr) const;
    virtual bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                              WavefrontIntersectionQueryTiming* timing = nullptr) const = 0;
    virtual std::vector<bool>
    intersectAnyBatch(const Scene& scene, const std::vector<WavefrontAnyHitQuery>& queries,
                      WavefrontIntersectionQueryTiming* timing = nullptr) const;
    virtual PrimitivePacketHit4
    intersectPacketClosest(const Scene& scene, const Ray4& rays,
                           const PrimitivePacketState4& states,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const = 0;
    virtual PrimitivePacketHit8
    intersectPacketClosest(const Scene& scene, const Ray8& rays,
                           const PrimitivePacketState8& states,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const = 0;

  protected:
    [[nodiscard]] const Primitive*
    intersectPreparedClosest(const Rayd& ray, HitPointInterval& hitPoints, State& state,
                             WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] WavefrontClosestHitResult
    intersectPreparedClosestResult(const Rayd& ray, State& state,
                                   WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] std::vector<WavefrontClosestHitResult>
    intersectPreparedClosestBatch(const std::vector<WavefrontClosestHitQuery>& queries,
                                  WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] bool
    intersectPreparedAny(const Rayd& ray, double maxDistance, State& state,
                         WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] std::vector<bool>
    intersectPreparedAnyBatch(const std::vector<WavefrontAnyHitQuery>& queries,
                              WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] PrimitivePacketHit4
    intersectPreparedPacketClosest(const Ray4& rays, const PrimitivePacketState4& states,
                                   WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] PrimitivePacketHit8
    intersectPreparedPacketClosest(const Ray8& rays, const PrimitivePacketState8& states,
                                   WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] bool packedClosestHitAvailable() const;
    [[nodiscard]] bool packedAnyHitAvailable() const;
    [[nodiscard]] virtual bool preparedPackedClosestHitAvailable() const;
    [[nodiscard]] virtual const char* preparedPackedClosestHitExecutionPath() const;
    [[nodiscard]] virtual std::vector<GpuIntersectionHitRecord>
    intersectPreparedPackedClosest(const std::vector<GpuIntersectionRay>& rays,
                                   WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] WavefrontClosestHitResult
    closestHitResultFromPackedRecord(const CompiledIntersectionScene& scene,
                                     const GpuIntersectionHitRecord& hit, State* state,
                                     const char* reason) const;
    [[nodiscard]] virtual bool preparedPackedAnyHitAvailable() const;
    [[nodiscard]] virtual const char* preparedPackedAnyHitExecutionPath() const;
    [[nodiscard]] virtual std::vector<GpuIntersectionOcclusionRecord>
    intersectPreparedPackedAny(const std::vector<GpuIntersectionRay>& rays,
                               WavefrontIntersectionQueryTiming* timing = nullptr) const;
    [[nodiscard]] bool preparedGpuTransferContractAvailable() const;
    [[nodiscard]] std::uint64_t estimatedTransferBytes(std::uint64_t submittedRays,
                                                       std::uint64_t bytesPerRay) const;
  };

  /**
    * @brief Canonical CPU backend that preserves the existing Scene traversal path.
    */
  class CpuWavefrontIntersectionBackend final : public WavefrontIntersectionBackend {
  public:
    static const CpuWavefrontIntersectionBackend& instance();

    const char* name() const override;
    const char* executionPath() const override;
    const Primitive*
    intersectClosest(const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints, State& state,
                     WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                      WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    std::vector<bool>
    intersectAnyBatch(const Scene& scene, const std::vector<WavefrontAnyHitQuery>& queries,
                      WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    PrimitivePacketHit4
    intersectPacketClosest(const Scene& scene, const Ray4& rays,
                           const PrimitivePacketState4& states,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    PrimitivePacketHit8
    intersectPacketClosest(const Scene& scene, const Ray8& rays,
                           const PrimitivePacketState8& states,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const override;
  };

  /**
    * @brief macOS GPU-intersection backend.
    *
    * Eligible prepared scenes execute Metal basic closest-hit and any-hit
    * kernels. Unsupported scenes, unavailable devices, or runtime kernel
    * failures fall back visibly to the packed CPU contract or the canonical
    * runtime CPU backend.
    */
  class MetalWavefrontIntersectionBackend final : public WavefrontIntersectionBackend {
  public:
    static const MetalWavefrontIntersectionBackend& instance();
    static std::shared_ptr<const WavefrontIntersectionBackend>
    createPrepared(std::shared_ptr<const CompiledIntersectionScene> scene,
                   std::string requestedName = "gpu");

    const char* platformName() const override;
    [[nodiscard]] bool isAvailable() const;

    const char* name() const override;
    const char* requestedName() const override;
    const char* availability() const override;
    const char* fallbackReason() const override;
    const char* executionPath() const override;
    const char* closestHitExecutionPath() const override;
    const char* anyHitExecutionPath() const override;
    bool platformGpuDeviceAvailable() const override;
    bool platformGpuRenderPathAvailable() const override;
    const CompiledIntersectionScene* compiledScene() const override;
    const GpuIntersectionSceneBuffers* gpuIntersectionSceneBuffers() const override;
    bool prefersClosestHitBatch(std::uint64_t submittedRays) const override;
    bool prefersAnyHitBatch(std::uint64_t submittedRays) const override;
    WavefrontClosestHitResult
    intersectClosestResult(const Scene& scene, const Rayd& ray, State& state,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    const Primitive*
    intersectClosest(const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints, State& state,
                     WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    std::vector<WavefrontClosestHitResult>
    intersectClosestBatch(const Scene& scene, const std::vector<WavefrontClosestHitQuery>& queries,
                          WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                      WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    std::vector<bool>
    intersectAnyBatch(const Scene& scene, const std::vector<WavefrontAnyHitQuery>& queries,
                      WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    PrimitivePacketHit4
    intersectPacketClosest(const Scene& scene, const Ray4& rays,
                           const PrimitivePacketState4& states,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    PrimitivePacketHit8
    intersectPacketClosest(const Scene& scene, const Ray8& rays,
                           const PrimitivePacketState8& states,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const override;

  protected:
    bool preparedPackedClosestHitAvailable() const override;
    const char* preparedPackedClosestHitExecutionPath() const override;
    std::vector<GpuIntersectionHitRecord> intersectPreparedPackedClosest(
      const std::vector<GpuIntersectionRay>& rays,
      WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    bool preparedPackedAnyHitAvailable() const override;
    const char* preparedPackedAnyHitExecutionPath() const override;
    std::vector<GpuIntersectionOcclusionRecord>
    intersectPreparedPackedAny(const std::vector<GpuIntersectionRay>& rays,
                               WavefrontIntersectionQueryTiming* timing = nullptr) const override;

  private:
    MetalWavefrontIntersectionBackend() = default;
    explicit MetalWavefrontIntersectionBackend(
      std::shared_ptr<const CompiledIntersectionScene> compiledScene,
      std::shared_ptr<const GpuIntersectionSceneBuffers> gpuSceneBuffers,
      std::shared_ptr<const MetalWavefrontPreparedScene> metalPreparedScene,
      std::string metalPreparationError, std::string requestedName);

    [[nodiscard]] std::string fallbackReasonText() const;
    [[nodiscard]] bool metalBasicHitAvailable() const;

    std::shared_ptr<const CompiledIntersectionScene> m_compiledScene;
    std::shared_ptr<const GpuIntersectionSceneBuffers> m_gpuSceneBuffers;
    std::shared_ptr<const MetalWavefrontPreparedScene> m_metalPreparedScene;
    std::string m_metalPreparationError;
    std::string m_requestedName;
  };

  /**
    * @brief Linux GPU-intersection backend.
    *
    * Eligible prepared scenes execute Vulkan compute basic closest-hit and
    * any-hit kernels. Unsupported scenes, unavailable devices, or runtime
    * kernel failures fall back visibly to the packed CPU contract or the
    * canonical runtime CPU backend.
    */
  class VulkanWavefrontIntersectionBackend final : public WavefrontIntersectionBackend {
  public:
    static const VulkanWavefrontIntersectionBackend& instance();
    static std::shared_ptr<const WavefrontIntersectionBackend>
    createPrepared(std::shared_ptr<const CompiledIntersectionScene> scene,
                   std::string requestedName = "gpu");
    [[nodiscard]] static bool supportsPackedScene(const GpuIntersectionSceneBuffers& buffers);

    const char* platformName() const override;
    [[nodiscard]] bool isAvailable() const;

    const char* name() const override;
    const char* requestedName() const override;
    const char* availability() const override;
    const char* fallbackReason() const override;
    const char* executionPath() const override;
    const char* closestHitExecutionPath() const override;
    const char* anyHitExecutionPath() const override;
    bool platformGpuDeviceAvailable() const override;
    bool platformGpuRenderPathAvailable() const override;
    const CompiledIntersectionScene* compiledScene() const override;
    const GpuIntersectionSceneBuffers* gpuIntersectionSceneBuffers() const override;
    bool prefersClosestHitBatch(std::uint64_t submittedRays) const override;
    bool prefersAnyHitBatch(std::uint64_t submittedRays) const override;
    WavefrontClosestHitResult
    intersectClosestResult(const Scene& scene, const Rayd& ray, State& state,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    const Primitive*
    intersectClosest(const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints, State& state,
                     WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    std::vector<WavefrontClosestHitResult>
    intersectClosestBatch(const Scene& scene, const std::vector<WavefrontClosestHitQuery>& queries,
                          WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                      WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    std::vector<bool>
    intersectAnyBatch(const Scene& scene, const std::vector<WavefrontAnyHitQuery>& queries,
                      WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    PrimitivePacketHit4
    intersectPacketClosest(const Scene& scene, const Ray4& rays,
                           const PrimitivePacketState4& states,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    PrimitivePacketHit8
    intersectPacketClosest(const Scene& scene, const Ray8& rays,
                           const PrimitivePacketState8& states,
                           WavefrontIntersectionQueryTiming* timing = nullptr) const override;

  protected:
    bool preparedPackedClosestHitAvailable() const override;
    const char* preparedPackedClosestHitExecutionPath() const override;
    std::vector<GpuIntersectionHitRecord> intersectPreparedPackedClosest(
      const std::vector<GpuIntersectionRay>& rays,
      WavefrontIntersectionQueryTiming* timing = nullptr) const override;
    bool preparedPackedAnyHitAvailable() const override;
    const char* preparedPackedAnyHitExecutionPath() const override;
    std::vector<GpuIntersectionOcclusionRecord>
    intersectPreparedPackedAny(const std::vector<GpuIntersectionRay>& rays,
                               WavefrontIntersectionQueryTiming* timing = nullptr) const override;

  private:
    VulkanWavefrontIntersectionBackend() = default;
    explicit VulkanWavefrontIntersectionBackend(
      std::shared_ptr<const CompiledIntersectionScene> compiledScene,
      std::shared_ptr<const GpuIntersectionSceneBuffers> gpuSceneBuffers,
      std::shared_ptr<const VulkanWavefrontPreparedScene> vulkanPreparedScene,
      std::string vulkanPreparationError, std::string requestedName);

    [[nodiscard]] std::string fallbackReasonText() const;
    [[nodiscard]] bool vulkanBasicHitAvailable() const;

    std::shared_ptr<const CompiledIntersectionScene> m_compiledScene;
    std::shared_ptr<const GpuIntersectionSceneBuffers> m_gpuSceneBuffers;
    std::shared_ptr<const VulkanWavefrontPreparedScene> m_vulkanPreparedScene;
    std::string m_vulkanPreparationError;
    std::string m_requestedName;
  };
}
