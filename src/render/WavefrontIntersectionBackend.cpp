#include "render/WavefrontIntersectionBackend.h"

#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalWavefrontSmokeKernel.h"
#endif
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
        WavefrontIntersectionSceneDiagnostics diagnostics = {})
          : m_requestedName(std::move(requestedName)),
            m_availability(std::move(availability)),
            m_fallbackReason(std::move(fallbackReason)),
            m_diagnostics(diagnostics) {
      }

      const char* name() const override {
        return CpuWavefrontIntersectionBackend::instance().name();
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

      const Primitive* intersectClosest(const Scene& scene, const Rayd& ray,
                                        HitPointInterval& hitPoints, State& state) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                            state);
      }

      bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance,
                        State& state) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance,
                                                                        state);
      }

      PrimitivePacketHit4
      intersectPacketClosest(const Scene& scene, const Ray4& rays,
                             const PrimitivePacketState4& states) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states);
      }

      PrimitivePacketHit8
      intersectPacketClosest(const Scene& scene, const Ray8& rays,
                             const PrimitivePacketState8& states) const override {
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states);
      }

    private:
      std::string m_requestedName;
      std::string m_availability;
      std::string m_fallbackReason;
      WavefrontIntersectionSceneDiagnostics m_diagnostics;
    };

    std::shared_ptr<const WavefrontIntersectionBackend>
    makeDelegatingBackend(std::string requestedName, std::string availability,
                          std::string fallbackReason,
                          WavefrontIntersectionSceneDiagnostics diagnostics = {}) {
      return std::make_shared<CpuDelegatingWavefrontIntersectionBackend>(
        std::move(requestedName), std::move(availability), std::move(fallbackReason), diagnostics);
    }

    std::shared_ptr<const WavefrontIntersectionBackend>
    staticBackend(const WavefrontIntersectionBackend& backend) {
      return std::shared_ptr<const WavefrontIntersectionBackend>(&backend, [](const auto*) {});
    }

    std::string gpuSceneUnsupportedReason(const CompiledIntersectionScene& scene) {
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

    const WavefrontIntersectionBackend& automaticCpuBackend() {
      static const CpuDelegatingWavefrontIntersectionBackend backend("auto", "available", "");
      return backend;
    }

    const WavefrontIntersectionBackend& gpuUnavailableBackend() {
#if defined(__APPLE__)
      return MetalWavefrontIntersectionBackend::instance();
#else
      return VulkanWavefrontIntersectionBackend::instance();
#endif
    }

    bool hostPlatformGpuBackendAvailable() {
#if defined(__APPLE__)
      return MetalWavefrontIntersectionBackend::instance().isAvailable();
#else
      return VulkanWavefrontIntersectionBackend::instance().isAvailable();
#endif
    }

    std::shared_ptr<const WavefrontIntersectionBackend>
    createPreparedGpuBackend(std::shared_ptr<const CompiledIntersectionScene> scene,
                             std::string requestedName) {
#if defined(__APPLE__)
      return MetalWavefrontIntersectionBackend::createPrepared(std::move(scene),
                                                               std::move(requestedName));
#else
      return VulkanWavefrontIntersectionBackend::createPrepared(std::move(scene),
                                                                std::move(requestedName));
#endif
    }
  }

  WavefrontIntersectionBackendAutoSelectionDecision
  WavefrontIntersectionBackendAutoSelectionPolicy::decide(
    bool platformGpuAvailable, const WavefrontIntersectionSceneDiagnostics& diagnostics,
    const WavefrontIntersectionBackendSelectionContext& context) const {
    if (!platformGpuAvailable) {
      return {false, "auto selected CPU: platform GPU intersection backend is unavailable"};
    }

    if (!sceneCanUseGpu(diagnostics)) {
      if (!diagnostics.compiled) {
        return {false, "auto selected CPU: intersection scene was not compiled"};
      }
      if (diagnostics.unsupportedPrimitives > 0) {
        return {false, "auto selected CPU: intersection scene contains unsupported primitives"};
      }
      return {false, "auto selected CPU: intersection scene is not basic-kernel eligible"};
    }

    if (!expectedRayCountJustifiesGpu(context)) {
      return {false, "auto selected CPU: expected ray count " +
                       std::to_string(context.expectedRayCount) + " is below GPU threshold " +
                       std::to_string(context.minimumGpuRayCount)};
    }

    return {true, "auto selected GPU: supported scene and expected ray count justify transfer"};
  }

  bool WavefrontIntersectionBackendAutoSelectionPolicy::sceneCanUseGpu(
    const WavefrontIntersectionSceneDiagnostics& diagnostics) const {
    return diagnostics.compiled && diagnostics.unsupportedPrimitives == 0 &&
           diagnostics.basicHitKernelEligible;
  }

  bool WavefrontIntersectionBackendAutoSelectionPolicy::expectedRayCountJustifiesGpu(
    const WavefrontIntersectionBackendSelectionContext& context) const {
    return context.expectedRayCount >= context.minimumGpuRayCount;
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
          policy.decide(false, diagnostics, context);
        std::string reason = decision.reason;
        reason += ": ";
        reason += gpuUnavailableBackend().fallbackReason();
        return makeDelegatingBackend("auto", "available", reason);
      }

      const auto compiled = std::make_shared<const CompiledIntersectionScene>(
        IntersectionSceneCompiler().compile(scene));
      const auto buffers = GpuIntersectionScenePacker().packScene(*compiled);
      const WavefrontIntersectionSceneDiagnostics diagnostics =
        WavefrontIntersectionSceneDiagnostics::fromCompiledSceneAndUploadBuffers(*compiled,
                                                                                 buffers);
      const WavefrontIntersectionBackendAutoSelectionDecision decision =
        policy.decide(true, diagnostics, context);
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
          WavefrontIntersectionSceneDiagnostics::fromCompiledScene(*compiled));
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
    diagnostics.transforms = scene.transforms().size();
    diagnostics.unsupportedPrimitives = scene.unsupportedPrimitives().size();
    diagnostics.uploadBytes = buffers.uploadByteCount();
    diagnostics.triangleClosestHitKernelEligible = buffers.triangleClosestHitKernelEligible();
    diagnostics.basicHitKernelEligible = buffers.basicHitKernelEligible();
    diagnostics.packedClosestHitKernelEligible = buffers.packedClosestHitKernelEligible();
    return diagnostics;
  }

  const char* WavefrontIntersectionBackend::requestedName() const {
    return name();
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

  bool WavefrontIntersectionBackend::packedClosestHitAvailable() const {
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    return buffers && buffers->packedClosestHitKernelEligible();
  }

  bool WavefrontIntersectionBackend::packedAnyHitAvailable() const {
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    return buffers && buffers->packedClosestHitKernelEligible();
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
    const std::vector<GpuIntersectionRay>& rays) const {
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    if (!buffers || !buffers->packedClosestHitKernelEligible()) {
      return {};
    }
    return GpuIntersectionIntersector().intersectClosest(*buffers, rays);
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
    const std::vector<GpuIntersectionRay>& rays) const {
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    if (!buffers || !buffers->packedClosestHitKernelEligible()) {
      return {};
    }

    std::vector<GpuIntersectionOcclusionRecord> records;
    records.reserve(rays.size());
    for (const GpuIntersectionRay& ray : rays) {
      GpuIntersectionOcclusionRecord record;
      record.occluded = GpuIntersectionIntersector().intersectAny(*buffers, ray) ? 1u : 0u;
      record.rayIndex = ray.rayIndex;
      records.push_back(record);
    }
    return records;
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
    const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (!scene) {
      state.miss(nullptr, "Compiled intersection scene unavailable");
      return nullptr;
    }

    if (preparedPackedClosestHitAvailable()) {
      const GpuIntersectionRay packedRay = GpuIntersectionScenePacker().packRay(ray, 0);
      const std::vector<GpuIntersectionHitRecord> hits =
        intersectPreparedPackedClosest({packedRay});
      if (hits.empty()) {
        state.miss(nullptr, "Packed GPU intersection scene");
        return nullptr;
      }
      const GpuIntersectionHitRecord& hit = hits.front();
      if (!hit.hit || hit.object >= scene->objects().size()) {
        state.miss(nullptr, "Packed GPU intersection scene");
        return nullptr;
      }

      const Primitive* primitive = scene->objects()[hit.object];
      const HitPoint hitPoint(
        primitive, hit.distance, Vector4d(hit.point[0], hit.point[1], hit.point[2], hit.point[3]),
        Vector3d(hit.normal[0], hit.normal[1], hit.normal[2]), Vector2d(hit.uv[0], hit.uv[1]));
      hitPoints.add(hitPoint);
      state.hit(primitive, "Packed GPU intersection scene");
      return primitive;
    }

    const CompiledIntersectionHit hit =
      CompiledIntersectionSceneIntersector().intersectClosest(*scene, ray);
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

  bool WavefrontIntersectionBackend::intersectPreparedAny(const Rayd& ray, double maxDistance,
                                                          State& state) const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (!scene) {
      state.shadowMiss(nullptr, "Compiled intersection scene unavailable");
      return false;
    }

    bool hit = false;
    const char* reason = "Compiled intersection scene";
    if (preparedPackedAnyHitAvailable()) {
      const GpuIntersectionRay packedRay =
        GpuIntersectionScenePacker().packRay(ray, 0, 0.0, maxDistance);
      const std::vector<GpuIntersectionOcclusionRecord> records =
        intersectPreparedPackedAny({packedRay});
      hit = !records.empty() && records.front().occluded != 0;
      reason = "Packed GPU intersection scene";
    } else {
      hit = CompiledIntersectionSceneIntersector().intersectAny(*scene, ray, maxDistance);
    }

    if (hit) {
      state.shadowHit(nullptr, reason);
    } else {
      state.shadowMiss(nullptr, reason);
    }
    return hit;
  }

  PrimitivePacketHit4 WavefrontIntersectionBackend::intersectPreparedPacketClosest(
    const Ray4& rays, const PrimitivePacketState4& states) const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (scene && preparedPackedClosestHitAvailable()) {
      std::vector<GpuIntersectionRay> packedRays;
      packedRays.reserve(Ray4::lanes);
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        packedRays.push_back(
          GpuIntersectionScenePacker().packRay(rays.rayd(lane), static_cast<std::uint32_t>(lane)));
      }

      const std::vector<GpuIntersectionHitRecord> hits = intersectPreparedPackedClosest(packedRays);

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
      const Primitive* primitive = intersectPreparedClosest(rays.rayd(lane), hitPoints, *state);
      const HitPoint& hitPoint = hitPoints.minWithPositiveDistance();
      if (primitive && !hitPoint.isUndefined()) {
        result.setHit(lane, primitive, hitPoint);
      }
    }
    return result;
  }

  PrimitivePacketHit8 WavefrontIntersectionBackend::intersectPreparedPacketClosest(
    const Ray8& rays, const PrimitivePacketState8& states) const {
    const CompiledIntersectionScene* scene = compiledScene();
    if (scene && preparedPackedClosestHitAvailable()) {
      std::vector<GpuIntersectionRay> packedRays;
      packedRays.reserve(Ray8::lanes);
      for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
        packedRays.push_back(
          GpuIntersectionScenePacker().packRay(rays.rayd(lane), static_cast<std::uint32_t>(lane)));
      }

      const std::vector<GpuIntersectionHitRecord> hits = intersectPreparedPackedClosest(packedRays);

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
      const Primitive* primitive = intersectPreparedClosest(rays.rayd(lane), hitPoints, *state);
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

  const Primitive* CpuWavefrontIntersectionBackend::intersectClosest(const Scene& scene,
                                                                     const Rayd& ray,
                                                                     HitPointInterval& hitPoints,
                                                                     State& state) const {
    return scene.intersect(ray, hitPoints, state);
  }

  bool CpuWavefrontIntersectionBackend::intersectAny(const Scene& scene, const Rayd& ray,
                                                     double maxDistance, State& state) const {
    return scene.occludes(ray, state, maxDistance);
  }

  PrimitivePacketHit4 CpuWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray4& rays, const PrimitivePacketState4& states) const {
    return scene.intersectPacketHits(rays, states);
  }

  PrimitivePacketHit8 CpuWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray8& rays, const PrimitivePacketState8& states) const {
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
    return std::shared_ptr<const WavefrontIntersectionBackend>(
      new MetalWavefrontIntersectionBackend(std::move(scene), std::move(buffers),
                                            std::move(requestedName)));
  }

  MetalWavefrontIntersectionBackend::MetalWavefrontIntersectionBackend(
    std::shared_ptr<const CompiledIntersectionScene> compiledScene,
    std::shared_ptr<const GpuIntersectionSceneBuffers> gpuSceneBuffers, std::string requestedName)
      : m_compiledScene(std::move(compiledScene)),
        m_gpuSceneBuffers(std::move(gpuSceneBuffers)),
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
    if (metalBasicHitAvailable()) {
      return "";
    }
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    if (!isAvailable()) {
      return "Metal wavefront intersection backend is enabled but no Metal device is available";
    }
    if (!gpuIntersectionSceneBuffers()) {
      return "Metal wavefront intersection backend is enabled but no prepared basic-hit scene is "
             "available";
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
    const std::vector<GpuIntersectionRay>& rays) const {
    if (!metalBasicHitAvailable()) {
      return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays);
    }
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    try {
      return MetalWavefrontSmokeKernel().runBasicClosestHitKernel(*gpuIntersectionSceneBuffers(),
                                                                  rays);
    } catch (const std::exception&) {
      return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays);
    }
#else
    return WavefrontIntersectionBackend::intersectPreparedPackedClosest(rays);
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
    const std::vector<GpuIntersectionRay>& rays) const {
    if (!metalBasicHitAvailable()) {
      return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays);
    }
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    try {
      return MetalWavefrontSmokeKernel().runBasicAnyHitKernel(*gpuIntersectionSceneBuffers(), rays);
    } catch (const std::exception&) {
      return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays);
    }
#else
    return WavefrontIntersectionBackend::intersectPreparedPackedAny(rays);
#endif
  }

  bool MetalWavefrontIntersectionBackend::metalBasicHitAvailable() const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const GpuIntersectionSceneBuffers* buffers = gpuIntersectionSceneBuffers();
    return buffers && buffers->basicHitKernelEligible() &&
           MetalWavefrontSmokeKernel().deviceAvailable();
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

  const Primitive* MetalWavefrontIntersectionBackend::intersectClosest(const Scene& scene,
                                                                       const Rayd& ray,
                                                                       HitPointInterval& hitPoints,
                                                                       State& state) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedClosest(ray, hitPoints, state);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                        state);
  }

  bool MetalWavefrontIntersectionBackend::intersectAny(const Scene& scene, const Rayd& ray,
                                                       double maxDistance, State& state) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedAny(ray, maxDistance, state);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance, state);
  }

  PrimitivePacketHit4 MetalWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray4& rays, const PrimitivePacketState4& states) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedPacketClosest(rays, states);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays, states);
  }

  PrimitivePacketHit8 MetalWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray8& rays, const PrimitivePacketState8& states) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedPacketClosest(rays, states);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays, states);
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
    return false;
  }

  const char* VulkanWavefrontIntersectionBackend::name() const {
    return CpuWavefrontIntersectionBackend::instance().name();
  }

  const char* VulkanWavefrontIntersectionBackend::requestedName() const {
    return m_requestedName.empty() ? "gpu" : m_requestedName.c_str();
  }

  const char* VulkanWavefrontIntersectionBackend::availability() const {
    return "fallback";
  }

  const char* VulkanWavefrontIntersectionBackend::fallbackReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return "Vulkan wavefront intersection backend is enabled but no render-path closest-hit kernel "
           "is built yet";
#else
    return "Vulkan wavefront intersection backend is not enabled in this build";
#endif
  }

  const char* VulkanWavefrontIntersectionBackend::executionPath() const {
    return closestHitExecutionPath();
  }

  const char* VulkanWavefrontIntersectionBackend::closestHitExecutionPath() const {
    if (packedClosestHitAvailable()) {
      return "packed_cpu";
    }
    return compiledScene() ? "compiled_cpu" : "runtime_scene";
  }

  const char* VulkanWavefrontIntersectionBackend::anyHitExecutionPath() const {
    if (packedAnyHitAvailable()) {
      return "packed_cpu";
    }
    return compiledScene() ? "compiled_cpu" : "runtime_scene";
  }

  const CompiledIntersectionScene* VulkanWavefrontIntersectionBackend::compiledScene() const {
    return m_compiledScene.get();
  }

  const GpuIntersectionSceneBuffers*
  VulkanWavefrontIntersectionBackend::gpuIntersectionSceneBuffers() const {
    return m_gpuSceneBuffers.get();
  }

  const Primitive* VulkanWavefrontIntersectionBackend::intersectClosest(const Scene& scene,
                                                                        const Rayd& ray,
                                                                        HitPointInterval& hitPoints,
                                                                        State& state) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedClosest(ray, hitPoints, state);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                        state);
  }

  bool VulkanWavefrontIntersectionBackend::intersectAny(const Scene& scene, const Rayd& ray,
                                                        double maxDistance, State& state) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedAny(ray, maxDistance, state);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance, state);
  }

  PrimitivePacketHit4 VulkanWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray4& rays, const PrimitivePacketState4& states) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedPacketClosest(rays, states);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays, states);
  }

  PrimitivePacketHit8 VulkanWavefrontIntersectionBackend::intersectPacketClosest(
    const Scene& scene, const Ray8& rays, const PrimitivePacketState8& states) const {
    if (compiledScene()) {
      (void)scene;
      return intersectPreparedPacketClosest(rays, states);
    }
    return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays, states);
  }
}
