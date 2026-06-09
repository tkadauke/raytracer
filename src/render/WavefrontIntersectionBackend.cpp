#include "render/WavefrontIntersectionBackend.h"

#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalWavefrontSmokeKernel.h"
#endif
#include "render/VulkanWavefrontSmokeKernel.h"
#include "render/State.h"
#include "render/primitives/Scene.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <utility>

namespace render {
  namespace {
    class CpuDelegatingWavefrontIntersectionBackend final : public WavefrontIntersectionBackend {
    public:
      CpuDelegatingWavefrontIntersectionBackend(
        std::string requestedName, std::string availability, std::string fallbackReason,
        WavefrontIntersectionSceneDiagnostics diagnostics = {}, std::string platformName = {})
          : m_requestedName(std::move(requestedName)),
            m_availability(std::move(availability)),
            m_fallbackReason(std::move(fallbackReason)),
            m_diagnostics(diagnostics),
            m_platformName(std::move(platformName)) {
      }

      const char* name() const override {
        return CpuWavefrontIntersectionBackend::instance().name();
      }

      const char* platformName() const override {
        return m_platformName.c_str();
      }

      const char* requestedName() const override {
        return m_requestedName.c_str();
      }

      const char* availability() const override {
        return m_availability.c_str();
      }

      const char* fallbackReason() const override {
        return m_fallbackReason.c_str();
      }

      const char* executionPath() const override {
        return "runtime_scene";
      }

      WavefrontIntersectionSceneDiagnostics compiledSceneDiagnostics() const override {
        return m_diagnostics;
      }

      const Primitive*
      intersectClosest(const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints,
                       State& state,
                       WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                            state, timing);
      }

      bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                        WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance,
                                                                        state, timing);
      }

      PrimitivePacketHit4
      intersectPacketClosest(const Scene& scene, const Ray4& rays,
                             const PrimitivePacketState4& states,
                             WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }

      PrimitivePacketHit8
      intersectPacketClosest(const Scene& scene, const Ray8& rays,
                             const PrimitivePacketState8& states,
                             WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }

    private:
      std::string m_requestedName;
      std::string m_availability;
      std::string m_fallbackReason;
      WavefrontIntersectionSceneDiagnostics m_diagnostics;
      std::string m_platformName;
    };
  }

  WavefrontIntersectionBackendAutoSelectionDecision
  WavefrontIntersectionBackendAutoSelectionPolicy::decide(
    bool platformGpuDeviceAvailable, bool platformGpuRenderPathAvailable,
    const WavefrontIntersectionSceneDiagnostics& diagnostics,
    const WavefrontIntersectionBackendSelectionContext& context) const {
    const std::uint64_t minimumRayCount = minimumExpectedRayCount(diagnostics, context);

    if (!platformGpuDeviceAvailable) {
      return {false, minimumRayCount,
              "auto selected CPU: platform GPU intersection backend is unavailable"};
    }

    if (!platformGpuRenderPathAvailable) {
      return {false, minimumRayCount,
              "auto selected CPU: platform GPU intersection render path is unavailable"};
    }

    if (!sceneCanUseGpu(diagnostics)) {
      if (!diagnostics.compiled) {
        return {false, minimumRayCount, "auto selected CPU: intersection scene was not compiled"};
      }
      if (diagnostics.unsupportedPrimitives > 0) {
        return {false, minimumRayCount,
                "auto selected CPU: intersection scene contains unsupported primitives"};
      }
      if (!diagnostics.packedClosestHitKernelEligible) {
        return {false, minimumRayCount,
                "auto selected CPU: intersection scene is not packed closest-hit eligible"};
      }
      if (!diagnostics.packedAnyHitKernelEligible) {
        return {false, minimumRayCount,
                "auto selected CPU: intersection scene is not packed any-hit eligible"};
      }
      return {false, minimumRayCount,
              "auto selected CPU: intersection scene is not packed-kernel eligible"};
    }

    if (!expectedRayCountJustifiesGpu(diagnostics, context)) {
      return {false, minimumRayCount,
              "auto selected CPU: expected ray count " + std::to_string(context.expectedRayCount) +
                " is below GPU threshold " + std::to_string(minimumRayCount) + " (scene upload " +
                std::to_string(diagnostics.uploadBytes) + " bytes)"};
    }

    return {true, minimumRayCount,
            "auto selected GPU: supported scene and expected ray count justify transfer"};
  }

  bool WavefrontIntersectionBackendAutoSelectionPolicy::sceneCanUseGpu(
    const WavefrontIntersectionSceneDiagnostics& diagnostics) const {
    return diagnostics.compiled && diagnostics.unsupportedPrimitives == 0 &&
           diagnostics.packedClosestHitKernelEligible && diagnostics.packedAnyHitKernelEligible;
  }

  bool WavefrontIntersectionBackendAutoSelectionPolicy::expectedRayCountJustifiesGpu(
    const WavefrontIntersectionSceneDiagnostics& diagnostics,
    const WavefrontIntersectionBackendSelectionContext& context) const {
    return context.expectedRayCount >= minimumExpectedRayCount(diagnostics, context);
  }

  std::uint64_t WavefrontIntersectionBackendAutoSelectionPolicy::minimumExpectedRayCount(
    const WavefrontIntersectionSceneDiagnostics& diagnostics,
    const WavefrontIntersectionBackendSelectionContext& context) const {
    return std::max(
      context.minimumGpuRayCount,
      saturatingProduct(sceneUploadKiB(diagnostics), context.minimumGpuRaysPerSceneUploadKiB));
  }

  std::uint64_t WavefrontIntersectionBackendAutoSelectionPolicy::sceneUploadKiB(
    const WavefrontIntersectionSceneDiagnostics& diagnostics) const {
    constexpr std::uint64_t bytesPerKiB = 1024;
    return diagnostics.uploadBytes / bytesPerKiB +
           (diagnostics.uploadBytes % bytesPerKiB == 0 ? 0 : 1);
  }

  std::uint64_t
  WavefrontIntersectionBackendAutoSelectionPolicy::saturatingProduct(std::uint64_t lhs,
                                                                     std::uint64_t rhs) const {
    constexpr std::uint64_t maxValue = std::numeric_limits<std::uint64_t>::max();
    if (lhs != 0 && rhs > maxValue / lhs) {
      return maxValue;
    }
    return lhs * rhs;
  }

  WavefrontIntersectionBackendChoice::WavefrontIntersectionBackendChoice()
      : m_kind(Kind::Auto) {
  }

  WavefrontIntersectionBackendChoice::WavefrontIntersectionBackendChoice(Kind kind)
      : m_kind(kind) {
  }

  WavefrontIntersectionBackendChoice WavefrontIntersectionBackendChoice::automatic() {
    return WavefrontIntersectionBackendChoice(Kind::Auto);
  }

  WavefrontIntersectionBackendChoice WavefrontIntersectionBackendChoice::cpu() {
    return WavefrontIntersectionBackendChoice(Kind::CPU);
  }

  WavefrontIntersectionBackendChoice WavefrontIntersectionBackendChoice::gpu() {
    return WavefrontIntersectionBackendChoice(Kind::GPU);
  }

  WavefrontIntersectionBackendChoice
  WavefrontIntersectionBackendChoice::fromString(std::string value) {
    value = normalized(std::move(value));
    if (value == "auto" || value == "automatic")
      return automatic();
    if (value == "cpu")
      return cpu();
    if (value == "gpu")
      return gpu();
    throw std::invalid_argument("wavefront intersection backend must be auto, cpu, or gpu");
  }

  WavefrontIntersectionBackendChoice::Kind WavefrontIntersectionBackendChoice::kind() const {
    return m_kind;
  }

  const char* WavefrontIntersectionBackendChoice::id() const {
    switch (m_kind) {
    case Kind::Auto:
      return "auto";
    case Kind::CPU:
      return "cpu";
    case Kind::GPU:
      return "gpu";
    }
    return "auto";
  }

  const WavefrontIntersectionBackend& WavefrontIntersectionBackendChoice::resolvedBackend() const {
    switch (m_kind) {
    case Kind::Auto:
      return automaticCpuBackend();
    case Kind::CPU:
      return CpuWavefrontIntersectionBackend::instance();
    case Kind::GPU:
      return gpuUnavailableBackend();
    }
    return automaticCpuBackend();
  }

  std::shared_ptr<const WavefrontIntersectionBackend>
  WavefrontIntersectionBackendChoice::createBackendForScene(const Scene& scene) const {
    return createBackendForScene(scene, WavefrontIntersectionBackendSelectionContext{});
  }

  std::shared_ptr<const WavefrontIntersectionBackend>
  WavefrontIntersectionBackendChoice::createBackendForScene(
    const Scene& scene, const WavefrontIntersectionBackendSelectionContext& context) const {
    switch (m_kind) {
    case Kind::Auto: {
      const WavefrontIntersectionBackendAutoSelectionPolicy policy;
      if (!hostPlatformGpuBackendAvailable()) {
        WavefrontIntersectionSceneDiagnostics diagnostics;
        const WavefrontIntersectionBackendAutoSelectionDecision decision =
          policy.decide(false, false, diagnostics, context);
        std::string reason = decision.reason;
        reason += ": ";
        reason += gpuUnavailableBackend().fallbackReason();
        return makeDelegatingBackend("auto", "available", reason, {},
                                     gpuUnavailableBackend().platformName());
      }

      if (!hostPlatformGpuRenderPathAvailable()) {
        WavefrontIntersectionSceneDiagnostics diagnostics;
        const WavefrontIntersectionBackendAutoSelectionDecision decision =
          policy.decide(true, false, diagnostics, context);
        std::string reason = decision.reason;
        reason += ": ";
        reason += gpuUnavailableBackend().fallbackReason();
        return makeDelegatingBackend("auto", "available", reason, {},
                                     gpuUnavailableBackend().platformName());
      }

      const auto compiled = std::make_shared<const CompiledIntersectionScene>(
        IntersectionSceneCompiler().compile(scene));
      const auto buffers = GpuIntersectionScenePacker().packScene(*compiled);
      const WavefrontIntersectionSceneDiagnostics diagnostics =
        WavefrontIntersectionSceneDiagnostics::fromCompiledSceneAndUploadBuffers(*compiled,
                                                                                 buffers);
#if !defined(__APPLE__)
      if (!VulkanWavefrontIntersectionBackend::supportsPackedScene(buffers)) {
        return makeDelegatingBackend(
          "auto", "available",
          "auto selected CPU: intersection scene is not eligible for Vulkan "
          "exact-primitive/static-transform hit kernels",
          diagnostics, gpuUnavailableBackend().platformName());
      }
#endif
      const WavefrontIntersectionBackendAutoSelectionDecision decision =
        policy.decide(true, true, diagnostics, context);
      if (!decision.useGpu) {
        return makeDelegatingBackend("auto", "available", decision.reason, diagnostics);
      }
      return createPreparedGpuBackend(compiled, "auto");
    }
    case Kind::CPU:
      return staticBackend(CpuWavefrontIntersectionBackend::instance());
    case Kind::GPU: {
      const auto compiled = std::make_shared<const CompiledIntersectionScene>(
        IntersectionSceneCompiler().compile(scene));
      if (!compiled->fullySupported()) {
        return makeDelegatingBackend(
          "gpu", "fallback", gpuSceneUnsupportedReason(*compiled),
          WavefrontIntersectionSceneDiagnostics::fromCompiledScene(*compiled),
          gpuUnavailableBackend().platformName());
      }
      return createPreparedGpuBackend(compiled, "gpu");
    }
    }

    return staticBackend(automaticCpuBackend());
  }

  bool WavefrontIntersectionBackendChoice::operator==(
    const WavefrontIntersectionBackendChoice& other) const {
    return m_kind == other.m_kind;
  }

  bool WavefrontIntersectionBackendChoice::operator!=(
    const WavefrontIntersectionBackendChoice& other) const {
    return !(*this == other);
  }

  std::string WavefrontIntersectionBackendChoice::normalized(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    value.erase(
      std::remove_if(value.begin(), value.end(),
                     [](char ch) { return ch == '_' || ch == '-' || ch == ',' || ch == ' '; }),
      value.end());
    return value;
  }

  std::shared_ptr<const WavefrontIntersectionBackend>
  WavefrontIntersectionBackendChoice::makeDelegatingBackend(
    std::string requestedName, std::string availability, std::string fallbackReason,
    WavefrontIntersectionSceneDiagnostics diagnostics, std::string platformName) const {
    return std::make_shared<CpuDelegatingWavefrontIntersectionBackend>(
      std::move(requestedName), std::move(availability), std::move(fallbackReason), diagnostics,
      std::move(platformName));
  }

  std::shared_ptr<const WavefrontIntersectionBackend>
  WavefrontIntersectionBackendChoice::staticBackend(
    const WavefrontIntersectionBackend& backend) const {
    return std::shared_ptr<const WavefrontIntersectionBackend>(&backend, [](const auto*) {});
  }

  std::string WavefrontIntersectionBackendChoice::gpuSceneUnsupportedReason(
    const CompiledIntersectionScene& scene) const {
    if (scene.unsupportedPrimitives().empty())
      return "GPU intersection scene is unsupported";

    const UnsupportedIntersectionPrimitive& unsupported = scene.unsupportedPrimitives().front();
    std::string result = "GPU intersection scene unsupported";
    if (!unsupported.primitiveName.empty()) {
      result += ": ";
      result += unsupported.primitiveName;
    }
    result += ": ";
    result += unsupported.reason;
    if (scene.unsupportedPrimitives().size() > 1) {
      result += " (";
      result += std::to_string(scene.unsupportedPrimitives().size());
      result += " unsupported leaves)";
    }
    return result;
  }

  const WavefrontIntersectionBackend&
  WavefrontIntersectionBackendChoice::automaticCpuBackend() const {
    static const CpuDelegatingWavefrontIntersectionBackend backend("auto", "available", "");
    return backend;
  }

  const WavefrontIntersectionBackend&
  WavefrontIntersectionBackendChoice::gpuUnavailableBackend() const {
#if defined(__APPLE__)
    return MetalWavefrontIntersectionBackend::instance();
#else
    return VulkanWavefrontIntersectionBackend::instance();
#endif
  }

  bool WavefrontIntersectionBackendChoice::hostPlatformGpuBackendAvailable() const {
#if defined(__APPLE__)
    return MetalWavefrontIntersectionBackend::instance().isAvailable();
#else
    return VulkanWavefrontIntersectionBackend::instance().isAvailable();
#endif
  }

  bool WavefrontIntersectionBackendChoice::hostPlatformGpuRenderPathAvailable() const {
#if defined(__APPLE__)
    return MetalWavefrontIntersectionBackend::instance().platformGpuRenderPathAvailable();
#else
    return VulkanWavefrontIntersectionBackend::instance().platformGpuRenderPathAvailable();
#endif
  }

  std::shared_ptr<const WavefrontIntersectionBackend>
  WavefrontIntersectionBackendChoice::createPreparedGpuBackend(
    std::shared_ptr<const CompiledIntersectionScene> scene, std::string requestedName) const {
#if defined(__APPLE__)
    return MetalWavefrontIntersectionBackend::createPrepared(std::move(scene),
                                                             std::move(requestedName));
#else
    return VulkanWavefrontIntersectionBackend::createPrepared(std::move(scene),
                                                              std::move(requestedName));
#endif
  }

  WavefrontIntersectionSceneDiagnostics
  WavefrontIntersectionSceneDiagnostics::fromCompiledScene(const CompiledIntersectionScene& scene) {
    return fromCompiledSceneAndUploadBuffers(scene, GpuIntersectionScenePacker().packScene(scene));
  }

  WavefrontIntersectionSceneDiagnostics
  WavefrontIntersectionSceneDiagnostics::fromCompiledSceneAndUploadBuffers(
    const CompiledIntersectionScene& scene, const GpuIntersectionSceneBuffers& buffers) {
    WavefrontIntersectionSceneDiagnostics diagnostics;
    diagnostics.compiled = true;
    diagnostics.bvhNodes = scene.bvh().size();
    diagnostics.primitives = scene.primitives().size();
    diagnostics.triangles = scene.triangles().size();
    diagnostics.spheres = scene.spheres().size();
    diagnostics.planes = scene.planes().size();
    diagnostics.rectangles = scene.rectangles().size();
    diagnostics.disks = scene.disks().size();
    diagnostics.openCylinders = scene.openCylinders().size();
    diagnostics.transforms = scene.transforms().size();
    diagnostics.unsupportedPrimitives = scene.unsupportedPrimitives().size();
    diagnostics.uploadBytes = buffers.uploadByteCount();
    diagnostics.triangleClosestHitKernelEligible = buffers.triangleClosestHitKernelEligible();
    diagnostics.basicHitKernelEligible = buffers.basicHitKernelEligible();
    diagnostics.packedClosestHitKernelEligible = buffers.packedClosestHitKernelEligible();
    diagnostics.packedAnyHitKernelEligible = buffers.packedAnyHitKernelEligible();
    return diagnostics;
  }

  const char* WavefrontIntersectionBackend::requestedName() const {
    return name();
  }

  const char* WavefrontIntersectionBackend::platformName() const {
    return "";
  }

  const char* WavefrontIntersectionBackend::availability() const {
    return "available";
  }

  const char* WavefrontIntersectionBackend::fallbackReason() const {
    return "";
  }

  const char* WavefrontIntersectionBackend::executionPath() const {
    return "runtime_scene";
  }

  const char* WavefrontIntersectionBackend::closestHitExecutionPath() const {
    return executionPath();
  }

  const char* WavefrontIntersectionBackend::anyHitExecutionPath() const {
    return executionPath();
  }

  bool WavefrontIntersectionBackend::platformGpuDeviceAvailable() const {
    return false;
  }

  bool WavefrontIntersectionBackend::platformGpuRenderPathAvailable() const {
    return false;
  }

  const CompiledIntersectionScene* WavefrontIntersectionBackend::compiledScene() const {
    return nullptr;
  }

  const GpuIntersectionSceneBuffers*
  WavefrontIntersectionBackend::gpuIntersectionSceneBuffers() const {
    return nullptr;
  }

  WavefrontIntersectionSceneDiagnostics
  WavefrontIntersectionBackend::compiledSceneDiagnostics() const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (!scene) {
      return {};
    }
    if (const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers()) {
      return WavefrontIntersectionSceneDiagnostics::fromCompiledSceneAndUploadBuffers(*scene,
                                                                                      *buffers);
    }
    return WavefrontIntersectionSceneDiagnostics::fromCompiledScene(*scene);
  }

  std::uint64_t WavefrontIntersectionBackend::estimatedClosestHitRayUploadBytes(
    std::uint64_t submittedRays) const {
    return estimatedTransferBytes(submittedRays, sizeof(GpuIntersectionRay));
  }

  std::uint64_t WavefrontIntersectionBackend::estimatedClosestHitReadbackBytes(
    std::uint64_t submittedRays) const {
    return estimatedTransferBytes(submittedRays, sizeof(GpuIntersectionHitRecord));
  }

  std::uint64_t
  WavefrontIntersectionBackend::estimatedAnyHitRayUploadBytes(std::uint64_t submittedRays) const {
    return estimatedTransferBytes(submittedRays, sizeof(GpuIntersectionRay));
  }

  std::uint64_t
  WavefrontIntersectionBackend::estimatedAnyHitReadbackBytes(std::uint64_t submittedRays) const {
    return estimatedTransferBytes(submittedRays, sizeof(GpuIntersectionOcclusionRecord));
  }

  bool WavefrontIntersectionBackend::prefersClosestHitBatch(std::uint64_t /*submittedRays*/) const {
    return false;
  }

  bool WavefrontIntersectionBackend::prefersAnyHitBatch(std::uint64_t /*submittedRays*/) const {
    return false;
  }

  std::vector<WavefrontClosestHitResult> WavefrontIntersectionBackend::intersectClosestBatch(
    const Scene& scene, const std::vector<WavefrontClosestHitQuery>& queries,
    WavefrontIntersectionQueryTiming* timing) const {
    std::vector<WavefrontClosestHitResult> results;
    results.reserve(queries.size());
    for (const WavefrontClosestHitQuery& query : queries) {
      State scratchState;
      State& state = query.state ? *query.state : scratchState;
      HitPointInterval hitPoints;
      WavefrontIntersectionQueryTiming queryTiming;
      const Primitive* primitive =
        intersectClosest(scene, query.ray, hitPoints, state, timing ? &queryTiming : nullptr);
      if (timing) {
        timing->add(queryTiming);
      }

      WavefrontClosestHitResult result;
      if (primitive) {
        const HitPoint& hitPoint = hitPoints.minWithPositiveDistance();
        if (!hitPoint.isUndefined()) {
          result.primitive = primitive;
          result.hitPoint = hitPoint;
        }
      }
      results.push_back(result);
    }
    return results;
  }

  std::vector<bool>
  WavefrontIntersectionBackend::intersectAnyBatch(const Scene& scene,
                                                  const std::vector<WavefrontAnyHitQuery>& queries,
                                                  WavefrontIntersectionQueryTiming* timing) const {
    std::vector<bool> results;
    results.reserve(queries.size());
    for (const WavefrontAnyHitQuery& query : queries) {
      State scratchState;
      State& state = query.state ? *query.state : scratchState;
      WavefrontIntersectionQueryTiming queryTiming;
      const bool hit =
        intersectAny(scene, query.ray, query.maxDistance, state, timing ? &queryTiming : nullptr);
      if (timing) {
        timing->add(queryTiming);
      }
      results.push_back(hit);
    }
    return results;
  }

  bool WavefrontIntersectionBackend::packedClosestHitAvailable() const {
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    return buffers && buffers->packedClosestHitKernelEligible();
  }

  bool WavefrontIntersectionBackend::packedAnyHitAvailable() const {
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    return buffers && buffers->packedAnyHitKernelEligible();
  }

  bool WavefrontIntersectionBackend::preparedPackedClosestHitAvailable() const {
    return packedClosestHitAvailable();
  }

  const char* WavefrontIntersectionBackend::preparedPackedClosestHitExecutionPath() const {
    if (preparedPackedClosestHitAvailable()) {
      return "packed_cpu";
    }
    return compiledScene() ? "compiled_cpu" : "runtime_scene";
  }

  std::vector<GpuIntersectionHitRecord>
  WavefrontIntersectionBackend::intersectPreparedPackedClosest(
    const std::vector<GpuIntersectionRay>& rays, WavefrontIntersectionQueryTiming* timing) const {
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    if (!buffers || !buffers->packedClosestHitKernelEligible()) {
      return {};
    }
    if (timing) {
      timing->recordExecutionPath("packed_cpu");
    }
    return GpuIntersectionIntersector().intersectClosest(*buffers, rays);
  }

  WavefrontClosestHitResult WavefrontIntersectionBackend::closestHitResultFromPackedRecord(
    const CompiledIntersectionScene& scene, const GpuIntersectionHitRecord& hit, State* state,
    const char* reason) const {
    WavefrontClosestHitResult result;
    if (!hit.hit || hit.object >= scene.objects().size()) {
      if (state) {
        state->miss(nullptr, reason);
      }
      return result;
    }

    const Primitive* primitive = scene.objects()[hit.object];
    result.primitive = primitive;
    if (hit.material < scene.materials().size()) {
      result.material = scene.materials()[hit.material];
    }
    result.hitPoint = HitPoint(
      primitive, hit.distance, Vector4d(hit.point[0], hit.point[1], hit.point[2], hit.point[3]),
      Vector3d(hit.normal[0], hit.normal[1], hit.normal[2]), Vector2d(hit.uv[0], hit.uv[1]));
    if (state) {
      state->hit(primitive, reason);
    }
    return result;
  }

  bool WavefrontIntersectionBackend::preparedPackedAnyHitAvailable() const {
    return packedAnyHitAvailable();
  }

  const char* WavefrontIntersectionBackend::preparedPackedAnyHitExecutionPath() const {
    if (preparedPackedAnyHitAvailable()) {
      return "packed_cpu";
    }
    return compiledScene() ? "compiled_cpu" : "runtime_scene";
  }

  std::vector<GpuIntersectionOcclusionRecord>
  WavefrontIntersectionBackend::intersectPreparedPackedAny(
    const std::vector<GpuIntersectionRay>& rays, WavefrontIntersectionQueryTiming* timing) const {
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    if (!buffers || !buffers->packedAnyHitKernelEligible()) {
      return {};
    }
    if (timing) {
      timing->recordExecutionPath("packed_cpu");
    }
    return GpuIntersectionIntersector().intersectAny(*buffers, rays);
  }

  bool WavefrontIntersectionBackend::preparedGpuTransferContractAvailable() const {
    return gpuIntersectionSceneBuffers() != nullptr;
  }

  std::uint64_t
  WavefrontIntersectionBackend::estimatedTransferBytes(std::uint64_t submittedRays,
                                                       std::uint64_t bytesPerRay) const {
    if (!preparedGpuTransferContractAvailable() || submittedRays == 0 || bytesPerRay == 0) {
      return 0;
    }

    constexpr std::uint64_t maxValue = std::numeric_limits<std::uint64_t>::max();
    if (submittedRays > maxValue / bytesPerRay) {
      return maxValue;
    }
    return submittedRays * bytesPerRay;
  }

  const Primitive* WavefrontIntersectionBackend::intersectPreparedClosest(
    const Rayd& ray, HitPointInterval& hitPoints, State& state,
    WavefrontIntersectionQueryTiming* timing) const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (!scene) {
      state.miss(nullptr, "Compiled intersection scene unavailable");
      return nullptr;
    }

    if (preparedPackedClosestHitAvailable()) {
      const GpuIntersectionRay packedRay = GpuIntersectionScenePacker().packRay(ray, 0);
      const std::vector<GpuIntersectionHitRecord> hits =
        intersectPreparedPackedClosest({packedRay}, timing);
      if (hits.empty()) {
        state.miss(nullptr, "Packed GPU intersection scene");
        return nullptr;
      }
      const WavefrontClosestHitResult result = closestHitResultFromPackedRecord(
        *scene, hits.front(), &state, "Packed GPU intersection scene");
      if (!result.hit()) {
        return nullptr;
      }
      hitPoints.add(result.hitPoint);
      return result.primitive;
    }

    const CompiledIntersectionHit hit =
      CompiledIntersectionSceneIntersector().intersectClosest(*scene, ray);
    if (timing) {
      timing->recordExecutionPath("compiled_cpu");
    }
    if (!hit.hit || hit.object >= scene->objects().size()) {
      state.miss(nullptr, "Compiled intersection scene");
      return nullptr;
    }

    const Primitive* primitive = scene->objects()[hit.object];
    const HitPoint hitPoint(primitive, hit.distance, hit.point, hit.normal, hit.uv);
    hitPoints.add(hitPoint);
    state.hit(primitive, "Compiled intersection scene");
    return primitive;
  }

  std::vector<WavefrontClosestHitResult>
  WavefrontIntersectionBackend::intersectPreparedClosestBatch(
    const std::vector<WavefrontClosestHitQuery>& queries,
    WavefrontIntersectionQueryTiming* timing) const {
    if (queries.empty()) {
      return {};
    }

    const CompiledIntersectionScene* scene = compiledScene();
    std::vector<WavefrontClosestHitResult> results(queries.size());
    if (!scene) {
      for (const WavefrontClosestHitQuery& query : queries) {
        if (query.state) {
          query.state->miss(nullptr, "Compiled intersection scene unavailable");
        }
      }
      return results;
    }

    if (preparedPackedClosestHitAvailable()) {
      std::vector<GpuIntersectionRay> packedRays;
      packedRays.reserve(queries.size());
      for (std::size_t index = 0; index != queries.size(); ++index) {
        packedRays.push_back(GpuIntersectionScenePacker().packRay(
          queries[index].ray, static_cast<std::uint32_t>(index)));
      }

      const std::vector<GpuIntersectionHitRecord> hits =
        intersectPreparedPackedClosest(packedRays, timing);
      for (const GpuIntersectionHitRecord& hit : hits) {
        if (hit.rayIndex < results.size()) {
          results[hit.rayIndex] = closestHitResultFromPackedRecord(
            *scene, hit, queries[hit.rayIndex].state, "Packed GPU intersection scene");
        }
      }
      return results;
    }

    for (std::size_t index = 0; index != queries.size(); ++index) {
      State scratchState;
      State& state = queries[index].state ? *queries[index].state : scratchState;
      HitPointInterval hitPoints;
      const Primitive* primitive =
        intersectPreparedClosest(queries[index].ray, hitPoints, state, timing);
      if (!primitive) {
        continue;
      }
      const HitPoint& hitPoint = hitPoints.minWithPositiveDistance();
      if (!hitPoint.isUndefined()) {
        results[index] = WavefrontClosestHitResult{primitive, primitive->material(), hitPoint};
      }
    }
    return results;
  }

  bool WavefrontIntersectionBackend::intersectPreparedAny(
    const Rayd& ray, double maxDistance, State& state,
    WavefrontIntersectionQueryTiming* timing) const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (!scene) {
      state.shadowMiss(nullptr, "Compiled intersection scene unavailable");
      return false;
    }

    bool hit = false;
    const char* reason = "Compiled intersection scene";
    if (preparedPackedAnyHitAvailable()) {
      const GpuIntersectionRay packedRay =
        GpuIntersectionScenePacker().packRay(ray, 0, Ray<float>::epsilon, maxDistance);
      const std::vector<GpuIntersectionOcclusionRecord> records =
        intersectPreparedPackedAny({packedRay}, timing);
      hit = !records.empty() && records.front().occluded != 0;
      reason = "Packed GPU intersection scene";
    } else {
      hit = CompiledIntersectionSceneIntersector().intersectAny(*scene, ray, maxDistance);
      if (timing) {
        timing->recordExecutionPath("compiled_cpu");
      }
    }

    if (hit) {
      state.shadowHit(nullptr, reason);
    } else {
      state.shadowMiss(nullptr, reason);
    }
    return hit;
  }

  std::vector<bool> WavefrontIntersectionBackend::intersectPreparedAnyBatch(
    const std::vector<WavefrontAnyHitQuery>& queries,
    WavefrontIntersectionQueryTiming* timing) const {
    const CompiledIntersectionScene* scene = compiledScene();
    std::vector<bool> results(queries.size(), false);
    if (!scene) {
      for (const WavefrontAnyHitQuery& query : queries) {
        if (query.state) {
          query.state->shadowMiss(nullptr, "Compiled intersection scene unavailable");
        }
      }
      return results;
    }

    const char* reason = "Compiled intersection scene";
    if (preparedPackedAnyHitAvailable()) {
      std::vector<GpuIntersectionRay> packedRays;
      packedRays.reserve(queries.size());
      for (std::size_t index = 0; index != queries.size(); ++index) {
        packedRays.push_back(GpuIntersectionScenePacker().packRay(
          queries[index].ray, static_cast<std::uint32_t>(index), Ray<float>::epsilon,
          queries[index].maxDistance));
      }
      const std::vector<GpuIntersectionOcclusionRecord> records =
        intersectPreparedPackedAny(packedRays, timing);
      for (const GpuIntersectionOcclusionRecord& record : records) {
        if (record.rayIndex < results.size()) {
          results[record.rayIndex] = record.occluded != 0;
        }
      }
      reason = "Packed GPU intersection scene";
    } else {
      if (timing) {
        timing->recordExecutionPath("compiled_cpu");
      }
      for (std::size_t index = 0; index != queries.size(); ++index) {
        results[index] = CompiledIntersectionSceneIntersector().intersectAny(
          *scene, queries[index].ray, queries[index].maxDistance);
      }
    }

    for (std::size_t index = 0; index != queries.size(); ++index) {
      if (!queries[index].state) {
        continue;
      }
      if (results[index]) {
        queries[index].state->shadowHit(nullptr, reason);
      } else {
        queries[index].state->shadowMiss(nullptr, reason);
      }
    }
    return results;
  }

  PrimitivePacketHit4 WavefrontIntersectionBackend::intersectPreparedPacketClosest(
    const Ray4& rays, const PrimitivePacketState4& states,
    WavefrontIntersectionQueryTiming* timing) const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (scene && preparedPackedClosestHitAvailable()) {
      std::vector<GpuIntersectionRay> packedRays;
      packedRays.reserve(Ray4::lanes);
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        packedRays.push_back(
          GpuIntersectionScenePacker().packRay(rays.rayd(lane), static_cast<std::uint32_t>(lane)));
      }

      const std::vector<GpuIntersectionHitRecord> hits =
        intersectPreparedPackedClosest(packedRays, timing);

      PrimitivePacketHit4 result;
      for (const GpuIntersectionHitRecord& hit : hits) {
        const std::size_t lane = hit.rayIndex;
        if (lane >= Ray4::lanes) {
          continue;
        }
        State* state = states[lane];
        if (!state) {
          continue;
        }
        if (!hit.hit || hit.object >= scene->objects().size()) {
          state->miss(nullptr, "Packed GPU intersection scene");
          continue;
        }

        const Primitive* primitive = scene->objects()[hit.object];
        const HitPoint hitPoint(
          primitive, hit.distance, Vector4d(hit.point[0], hit.point[1], hit.point[2], hit.point[3]),
          Vector3d(hit.normal[0], hit.normal[1], hit.normal[2]), Vector2d(hit.uv[0], hit.uv[1]));
        result.setHit(lane, primitive, hitPoint);
        state->hit(primitive, "Packed GPU intersection scene");
      }
      return result;
    }

    PrimitivePacketHit4 result;
    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      State* state = states[lane];
      if (!state) {
        continue;
      }

      HitPointInterval hitPoints;
      const Primitive* primitive =
        intersectPreparedClosest(rays.rayd(lane), hitPoints, *state, timing);
      const HitPoint& hitPoint = hitPoints.minWithPositiveDistance();
      if (primitive && !hitPoint.isUndefined()) {
        result.setHit(lane, primitive, hitPoint);
      }
    }
    return result;
  }

  PrimitivePacketHit8 WavefrontIntersectionBackend::intersectPreparedPacketClosest(
    const Ray8& rays, const PrimitivePacketState8& states,
    WavefrontIntersectionQueryTiming* timing) const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (scene && preparedPackedClosestHitAvailable()) {
      std::vector<GpuIntersectionRay> packedRays;
      packedRays.reserve(Ray8::lanes);
      for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
        packedRays.push_back(
          GpuIntersectionScenePacker().packRay(rays.rayd(lane), static_cast<std::uint32_t>(lane)));
      }

      const std::vector<GpuIntersectionHitRecord> hits =
        intersectPreparedPackedClosest(packedRays, timing);

      PrimitivePacketHit8 result;
      for (const GpuIntersectionHitRecord& hit : hits) {
        const std::size_t lane = hit.rayIndex;
        if (lane >= Ray8::lanes) {
          continue;
        }
        State* state = states[lane];
        if (!state) {
          continue;
        }
        if (!hit.hit || hit.object >= scene->objects().size()) {
          state->miss(nullptr, "Packed GPU intersection scene");
          continue;
        }

        const Primitive* primitive = scene->objects()[hit.object];
        const HitPoint hitPoint(
          primitive, hit.distance, Vector4d(hit.point[0], hit.point[1], hit.point[2], hit.point[3]),
          Vector3d(hit.normal[0], hit.normal[1], hit.normal[2]), Vector2d(hit.uv[0], hit.uv[1]));
        result.setHit(lane, primitive, hitPoint);
        state->hit(primitive, "Packed GPU intersection scene");
      }
      return result;
    }

    PrimitivePacketHit8 result;
    for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
      State* state = states[lane];
      if (!state) {
        continue;
      }

      HitPointInterval hitPoints;
      const Primitive* primitive =
        intersectPreparedClosest(rays.rayd(lane), hitPoints, *state, timing);
      const HitPoint& hitPoint = hitPoints.minWithPositiveDistance();
      if (primitive && !hitPoint.isUndefined()) {
        result.setHit(lane, primitive, hitPoint);
      }
    }
    return result;
  }

  const CpuWavefrontIntersectionBackend& CpuWavefrontIntersectionBackend::instance() {
    static const CpuWavefrontIntersectionBackend backend;
    return backend;
  }

  const char* CpuWavefrontIntersectionBackend::name() const {
    return "cpu";
  }

  const char* CpuWavefrontIntersectionBackend::executionPath() const {
    return "runtime_scene";
  }

  const Primitive* CpuWavefrontIntersectionBackend::intersectClosest(
    const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints, State& state,
    WavefrontIntersectionQueryTiming* timing) const {
    if (timing) {
      timing->recordExecutionPath("runtime_scene");
    }
    return scene.intersect(ray, hitPoints, state);
  }

  bool
  CpuWavefrontIntersectionBackend::intersectAny(const Scene& scene, const Rayd& ray,
                                                double maxDistance, State& state,
                                                WavefrontIntersectionQueryTiming* timing) const {
    if (timing) {
      timing->recordExecutionPath("runtime_scene");
    }
    return scene.occludes(ray, state, maxDistance);
  }

  std::vector<bool> CpuWavefrontIntersectionBackend::intersectAnyBatch(
    const Scene& scene, const std::vector<WavefrontAnyHitQuery>& queries,
    WavefrontIntersectionQueryTiming* timing) const {
    return WavefrontIntersectionBackend::intersectAnyBatch(scene, queries, timing);
  }

  PrimitivePacketHit4 CpuWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray4& rays, const PrimitivePacketState4& states,
    WavefrontIntersectionQueryTiming* timing) const {
    if (timing) {
      timing->recordExecutionPath("runtime_scene");
    }
    return scene.intersectPacketHits(rays, states);
  }

  PrimitivePacketHit8 CpuWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray8& rays, const PrimitivePacketState8& states,
    WavefrontIntersectionQueryTiming* timing) const {
    if (timing) {
      timing->recordExecutionPath("runtime_scene");
    }
    return scene.intersectPacketHits(rays, states);
  }

  const MetalWavefrontIntersectionBackend& MetalWavefrontIntersectionBackend::instance() {
    static const MetalWavefrontIntersectionBackend backend;
    return backend;
  }

  std::shared_ptr<const WavefrontIntersectionBackend>
  MetalWavefrontIntersectionBackend::createPrepared(
    std::shared_ptr<const CompiledIntersectionScene> scene, std::string requestedName) {
    auto buffers = std::make_shared<const GpuIntersectionSceneBuffers>(
      GpuIntersectionScenePacker().packScene(*scene));
    std::shared_ptr<const MetalWavefrontPreparedScene> metalPreparedScene;
    std::string metalPreparationError;
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    if (buffers->basicHitKernelEligible() && MetalWavefrontSmokeKernel().renderPathAvailable()) {
      try {
        metalPreparedScene = std::make_shared<MetalWavefrontPreparedScene>(*buffers);
      } catch (const std::exception& e) {
        metalPreparationError = "Metal wavefront intersection backend failed to prepare scene "
                                "buffers: ";
        metalPreparationError += e.what();
      }
    }
#endif
    return std::shared_ptr<const WavefrontIntersectionBackend>(
      new MetalWavefrontIntersectionBackend(
        std::move(scene), std::move(buffers), std::move(metalPreparedScene),
        std::move(metalPreparationError), std::move(requestedName)));
  }

  MetalWavefrontIntersectionBackend::MetalWavefrontIntersectionBackend(
    std::shared_ptr<const CompiledIntersectionScene> compiledScene,
    std::shared_ptr<const GpuIntersectionSceneBuffers> gpuSceneBuffers,
    std::shared_ptr<const MetalWavefrontPreparedScene> metalPreparedScene,
    std::string metalPreparationError, std::string requestedName)
      : m_compiledScene(std::move(compiledScene)),
        m_gpuSceneBuffers(std::move(gpuSceneBuffers)),
        m_metalPreparedScene(std::move(metalPreparedScene)),
        m_metalPreparationError(std::move(metalPreparationError)),
        m_requestedName(std::move(requestedName)) {
  }

  const char* MetalWavefrontIntersectionBackend::platformName() const {
    return "metal";
  }

  bool MetalWavefrontIntersectionBackend::isAvailable() const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    return MetalWavefrontSmokeKernel().deviceAvailable();
#else
    return false;
#endif
  }

  const char* MetalWavefrontIntersectionBackend::name() const {
    if (metalBasicHitAvailable()) {
      return platformName();
    }
    return CpuWavefrontIntersectionBackend::instance().name();
  }

  const char* MetalWavefrontIntersectionBackend::requestedName() const {
    return m_requestedName.empty() ? "gpu" : m_requestedName.c_str();
  }

  const char* MetalWavefrontIntersectionBackend::availability() const {
    if (metalBasicHitAvailable()) {
      return "available";
    }
    return "fallback";
  }

  const char* MetalWavefrontIntersectionBackend::fallbackReason() const {
    static thread_local std::string reason;
    reason = fallbackReasonText();
    return reason.c_str();
  }

  std::string MetalWavefrontIntersectionBackend::fallbackReasonText() const {
    if (metalBasicHitAvailable()) {
      return "";
    }
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    if (!isAvailable()) {
      std::string reason =
        "Metal wavefront intersection backend is enabled but no Metal device is available";
      const std::string detail = MetalWavefrontSmokeKernel().deviceUnavailableReason();
      if (!detail.empty()) {
        reason += ": ";
        reason += detail;
      }
      return reason;
    }
    if (!platformGpuRenderPathAvailable()) {
      std::string reason =
        "Metal wavefront intersection backend is enabled but no render-path basic hit kernel is "
        "available";
      const std::string detail = MetalWavefrontSmokeKernel().renderPathUnavailableReason();
      if (!detail.empty()) {
        reason += ": ";
        reason += detail;
      }
      return reason;
    }
    if (!gpuIntersectionSceneBuffers()) {
      return "Metal wavefront intersection backend is enabled but no prepared basic-hit scene is "
             "available";
    }
    if (!m_metalPreparationError.empty()) {
      return m_metalPreparationError;
    }
    return "Metal wavefront intersection backend is enabled but the prepared scene is not "
           "eligible for the Metal basic hit kernel";
#else
    return "Metal wavefront intersection backend is not enabled in this build";
#endif
  }

  const char* MetalWavefrontIntersectionBackend::executionPath() const {
    return closestHitExecutionPath();
  }

  const char* MetalWavefrontIntersectionBackend::closestHitExecutionPath() const {
    return preparedPackedClosestHitExecutionPath();
  }

  const char* MetalWavefrontIntersectionBackend::anyHitExecutionPath() const {
    return preparedPackedAnyHitExecutionPath();
  }

  bool MetalWavefrontIntersectionBackend::platformGpuDeviceAvailable() const {
    return isAvailable();
  }

  bool MetalWavefrontIntersectionBackend::platformGpuRenderPathAvailable() const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    return MetalWavefrontSmokeKernel().renderPathAvailable();
#else
    return false;
#endif
  }

  bool MetalWavefrontIntersectionBackend::preparedPackedClosestHitAvailable() const {
    return metalBasicHitAvailable() ||
           WavefrontIntersectionBackend::preparedPackedClosestHitAvailable();
  }

  const char* MetalWavefrontIntersectionBackend::preparedPackedClosestHitExecutionPath() const {
    if (metalBasicHitAvailable()) {
      return "metal";
    }
    return WavefrontIntersectionBackend::preparedPackedClosestHitExecutionPath();
  }

  std::vector<GpuIntersectionHitRecord>
  MetalWavefrontIntersectionBackend::intersectPreparedPackedClosest(
    const std::vector<GpuIntersectionRay>& rays, WavefrontIntersectionQueryTiming* timing) const {
    if (!metalBasicHitAvailable()) {
      return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays, timing);
    }
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    try {
      const MetalWavefrontClosestHitKernelResult result =
        m_metalPreparedScene->runTimedBasicClosestHitKernel(rays);
      if (timing) {
        timing->add(result.timing);
        timing->recordExecutionPath("metal");
      }
      return result.hits;
    } catch (const std::exception& e) {
      if (timing) {
        timing->recordFallbackReason(std::string("Metal closest-hit kernel failed: ") + e.what());
      }
      return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays, timing);
    }
#else
    return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays, timing);
#endif
  }

  bool MetalWavefrontIntersectionBackend::preparedPackedAnyHitAvailable() const {
    return metalBasicHitAvailable() ||
           WavefrontIntersectionBackend::preparedPackedAnyHitAvailable();
  }

  const char* MetalWavefrontIntersectionBackend::preparedPackedAnyHitExecutionPath() const {
    if (metalBasicHitAvailable()) {
      return "metal";
    }
    return WavefrontIntersectionBackend::preparedPackedAnyHitExecutionPath();
  }

  std::vector<GpuIntersectionOcclusionRecord>
  MetalWavefrontIntersectionBackend::intersectPreparedPackedAny(
    const std::vector<GpuIntersectionRay>& rays, WavefrontIntersectionQueryTiming* timing) const {
    if (!metalBasicHitAvailable()) {
      return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays, timing);
    }
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    try {
      const MetalWavefrontAnyHitKernelResult result =
        m_metalPreparedScene->runTimedBasicAnyHitKernel(rays);
      if (timing) {
        timing->add(result.timing);
        timing->recordExecutionPath("metal");
      }
      return result.records;
    } catch (const std::exception& e) {
      if (timing) {
        timing->recordFallbackReason(std::string("Metal any-hit kernel failed: ") + e.what());
      }
      return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays, timing);
    }
#else
    return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays, timing);
#endif
  }

  bool MetalWavefrontIntersectionBackend::metalBasicHitAvailable() const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    return m_metalPreparedScene != nullptr && platformGpuRenderPathAvailable();
#else
    return false;
#endif
  }

  const CompiledIntersectionScene* MetalWavefrontIntersectionBackend::compiledScene() const {
    return m_compiledScene.get();
  }

  const GpuIntersectionSceneBuffers*
  MetalWavefrontIntersectionBackend::gpuIntersectionSceneBuffers() const {
    return m_gpuSceneBuffers.get();
  }

  bool
  MetalWavefrontIntersectionBackend::prefersClosestHitBatch(std::uint64_t submittedRays) const {
    return submittedRays > 1 && preparedPackedClosestHitAvailable();
  }

  bool MetalWavefrontIntersectionBackend::prefersAnyHitBatch(std::uint64_t submittedRays) const {
    return submittedRays > 0 && preparedPackedAnyHitAvailable();
  }

  const Primitive* MetalWavefrontIntersectionBackend::intersectClosest(
    const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints, State& state,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedClosest(ray, hitPoints, state, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                        state, timing);
  }

  std::vector<WavefrontClosestHitResult> MetalWavefrontIntersectionBackend::intersectClosestBatch(
    const Scene& scene, const std::vector<WavefrontClosestHitQuery>& queries,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedClosestBatch(queries, timing);
    }
    return WavefrontIntersectionBackend::intersectClosestBatch(scene, queries, timing);
  }

  bool
  MetalWavefrontIntersectionBackend::intersectAny(const Scene& scene, const Rayd& ray,
                                                  double maxDistance, State& state,
                                                  WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedAny(ray, maxDistance, state, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance, state,
                                                                    timing);
  }

  std::vector<bool> MetalWavefrontIntersectionBackend::intersectAnyBatch(
    const Scene& scene, const std::vector<WavefrontAnyHitQuery>& queries,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedAnyBatch(queries, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectAnyBatch(scene, queries, timing);
  }

  PrimitivePacketHit4 MetalWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray4& rays, const PrimitivePacketState4& states,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedPacketClosest(rays, states, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays, states,
                                                                              timing);
  }

  PrimitivePacketHit8 MetalWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray8& rays, const PrimitivePacketState8& states,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedPacketClosest(rays, states, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays, states,
                                                                              timing);
  }

  const VulkanWavefrontIntersectionBackend& VulkanWavefrontIntersectionBackend::instance() {
    static const VulkanWavefrontIntersectionBackend backend;
    return backend;
  }

  std::shared_ptr<const WavefrontIntersectionBackend>
  VulkanWavefrontIntersectionBackend::createPrepared(
    std::shared_ptr<const CompiledIntersectionScene> scene, std::string requestedName) {
    auto buffers = std::make_shared<const GpuIntersectionSceneBuffers>(
      GpuIntersectionScenePacker().packScene(*scene));
    return std::shared_ptr<const WavefrontIntersectionBackend>(
      new VulkanWavefrontIntersectionBackend(std::move(scene), std::move(buffers),
                                             std::move(requestedName)));
  }

  bool VulkanWavefrontIntersectionBackend::supportsPackedScene(
    const GpuIntersectionSceneBuffers& buffers) {
    return buffers.basicHitKernelEligible();
  }

  VulkanWavefrontIntersectionBackend::VulkanWavefrontIntersectionBackend(
    std::shared_ptr<const CompiledIntersectionScene> compiledScene,
    std::shared_ptr<const GpuIntersectionSceneBuffers> gpuSceneBuffers, std::string requestedName)
      : m_compiledScene(std::move(compiledScene)),
        m_gpuSceneBuffers(std::move(gpuSceneBuffers)),
        m_requestedName(std::move(requestedName)) {
  }

  const char* VulkanWavefrontIntersectionBackend::platformName() const {
    return "vulkan";
  }

  bool VulkanWavefrontIntersectionBackend::isAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanWavefrontSmokeKernel().deviceAvailable();
#else
    return false;
#endif
  }

  const char* VulkanWavefrontIntersectionBackend::name() const {
    if (vulkanBasicHitAvailable()) {
      return platformName();
    }
    return CpuWavefrontIntersectionBackend::instance().name();
  }

  const char* VulkanWavefrontIntersectionBackend::requestedName() const {
    return m_requestedName.empty() ? "gpu" : m_requestedName.c_str();
  }

  const char* VulkanWavefrontIntersectionBackend::availability() const {
    if (vulkanBasicHitAvailable()) {
      return "available";
    }
    return "fallback";
  }

  const char* VulkanWavefrontIntersectionBackend::fallbackReason() const {
    static thread_local std::string reason;
    reason = fallbackReasonText();
    return reason.c_str();
  }

  std::string VulkanWavefrontIntersectionBackend::fallbackReasonText() const {
    if (vulkanBasicHitAvailable()) {
      return "";
    }
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanWavefrontSmokeKernel kernel;
    if (!kernel.deviceAvailable()) {
      std::string reason =
        "Vulkan wavefront intersection backend is enabled but no Vulkan compute device is "
        "available";
      const std::string detail = kernel.deviceUnavailableReason();
      if (!detail.empty()) {
        reason += ": ";
        reason += detail;
      }
      return reason;
    }
    if (!kernel.renderPathAvailable()) {
      std::string reason = "Vulkan wavefront intersection backend is enabled but no render-path "
                           "exact-primitive hit kernels are available";
      const std::string detail = kernel.renderPathUnavailableReason();
      if (!detail.empty()) {
        reason += ": ";
        reason += detail;
      }
      return reason;
    }
    if (!gpuIntersectionSceneBuffers()) {
      return "Vulkan wavefront intersection backend is enabled but no prepared "
             "exact-primitive/static-transform scene is available";
    }
    return "Vulkan wavefront intersection backend is enabled but the prepared scene is not "
           "eligible for the Vulkan exact-primitive/static-transform hit kernels";
#else
    return "Vulkan wavefront intersection backend is not enabled in this build";
#endif
  }

  const char* VulkanWavefrontIntersectionBackend::executionPath() const {
    return closestHitExecutionPath();
  }

  const char* VulkanWavefrontIntersectionBackend::closestHitExecutionPath() const {
    return preparedPackedClosestHitExecutionPath();
  }

  const char* VulkanWavefrontIntersectionBackend::anyHitExecutionPath() const {
    return preparedPackedAnyHitExecutionPath();
  }

  bool VulkanWavefrontIntersectionBackend::platformGpuDeviceAvailable() const {
    return VulkanWavefrontSmokeKernel().deviceAvailable();
  }

  bool VulkanWavefrontIntersectionBackend::platformGpuRenderPathAvailable() const {
    return VulkanWavefrontSmokeKernel().renderPathAvailable();
  }

  bool VulkanWavefrontIntersectionBackend::preparedPackedClosestHitAvailable() const {
    return vulkanBasicHitAvailable() ||
           WavefrontIntersectionBackend::preparedPackedClosestHitAvailable();
  }

  const char* VulkanWavefrontIntersectionBackend::preparedPackedClosestHitExecutionPath() const {
    if (vulkanBasicHitAvailable()) {
      return "vulkan";
    }
    return WavefrontIntersectionBackend::preparedPackedClosestHitExecutionPath();
  }

  std::vector<GpuIntersectionHitRecord>
  VulkanWavefrontIntersectionBackend::intersectPreparedPackedClosest(
    const std::vector<GpuIntersectionRay>& rays, WavefrontIntersectionQueryTiming* timing) const {
    if (!vulkanBasicHitAvailable()) {
      return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays, timing);
    }
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    try {
      const VulkanWavefrontClosestHitKernelResult result =
        VulkanWavefrontSmokeKernel().runTimedBasicClosestHitKernel(*m_gpuSceneBuffers, rays);
      if (timing) {
        timing->add(result.timing);
        timing->recordExecutionPath("vulkan");
      }
      return result.hits;
    } catch (const std::exception& e) {
      if (timing) {
        timing->recordFallbackReason(std::string("Vulkan closest-hit kernel failed: ") + e.what());
      }
      return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays, timing);
    }
#else
    return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays, timing);
#endif
  }

  bool VulkanWavefrontIntersectionBackend::preparedPackedAnyHitAvailable() const {
    return vulkanBasicHitAvailable() ||
           WavefrontIntersectionBackend::preparedPackedAnyHitAvailable();
  }

  const char* VulkanWavefrontIntersectionBackend::preparedPackedAnyHitExecutionPath() const {
    if (vulkanBasicHitAvailable()) {
      return "vulkan";
    }
    return WavefrontIntersectionBackend::preparedPackedAnyHitExecutionPath();
  }

  std::vector<GpuIntersectionOcclusionRecord>
  VulkanWavefrontIntersectionBackend::intersectPreparedPackedAny(
    const std::vector<GpuIntersectionRay>& rays, WavefrontIntersectionQueryTiming* timing) const {
    if (!vulkanBasicHitAvailable()) {
      return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays, timing);
    }
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    try {
      const VulkanWavefrontAnyHitKernelResult result =
        VulkanWavefrontSmokeKernel().runTimedBasicAnyHitKernel(*m_gpuSceneBuffers, rays);
      if (timing) {
        timing->add(result.timing);
        timing->recordExecutionPath("vulkan");
      }
      return result.records;
    } catch (const std::exception& e) {
      if (timing) {
        timing->recordFallbackReason(std::string("Vulkan any-hit kernel failed: ") + e.what());
      }
      return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays, timing);
    }
#else
    return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays, timing);
#endif
  }

  const CompiledIntersectionScene* VulkanWavefrontIntersectionBackend::compiledScene() const {
    return m_compiledScene.get();
  }

  const GpuIntersectionSceneBuffers*
  VulkanWavefrontIntersectionBackend::gpuIntersectionSceneBuffers() const {
    return m_gpuSceneBuffers.get();
  }

  bool
  VulkanWavefrontIntersectionBackend::prefersClosestHitBatch(std::uint64_t submittedRays) const {
    return submittedRays > 1 && preparedPackedClosestHitAvailable();
  }

  bool VulkanWavefrontIntersectionBackend::prefersAnyHitBatch(std::uint64_t submittedRays) const {
    return submittedRays > 0 && preparedPackedAnyHitAvailable();
  }

  bool VulkanWavefrontIntersectionBackend::vulkanBasicHitAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return m_gpuSceneBuffers != nullptr && supportsPackedScene(*m_gpuSceneBuffers) &&
           platformGpuRenderPathAvailable();
#else
    return false;
#endif
  }

  const Primitive* VulkanWavefrontIntersectionBackend::intersectClosest(
    const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints, State& state,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedClosest(ray, hitPoints, state, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                        state, timing);
  }

  std::vector<WavefrontClosestHitResult> VulkanWavefrontIntersectionBackend::intersectClosestBatch(
    const Scene& scene, const std::vector<WavefrontClosestHitQuery>& queries,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedClosestBatch(queries, timing);
    }
    return WavefrontIntersectionBackend::intersectClosestBatch(scene, queries, timing);
  }

  bool
  VulkanWavefrontIntersectionBackend::intersectAny(const Scene& scene, const Rayd& ray,
                                                   double maxDistance, State& state,
                                                   WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedAny(ray, maxDistance, state, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance, state,
                                                                    timing);
  }

  std::vector<bool> VulkanWavefrontIntersectionBackend::intersectAnyBatch(
    const Scene& scene, const std::vector<WavefrontAnyHitQuery>& queries,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedAnyBatch(queries, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectAnyBatch(scene, queries, timing);
  }

  PrimitivePacketHit4 VulkanWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray4& rays, const PrimitivePacketState4& states,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedPacketClosest(rays, states, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays, states,
                                                                              timing);
  }

  PrimitivePacketHit8 VulkanWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray8& rays, const PrimitivePacketState8& states,
    WavefrontIntersectionQueryTiming* timing) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedPacketClosest(rays, states, timing);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays, states,
                                                                              timing);
  }
}
