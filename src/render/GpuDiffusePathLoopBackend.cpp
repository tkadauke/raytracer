#include "render/GpuDiffusePathLoopBackend.h"

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalGpuDiffusePathFrontierCompactionBackend.h"
#include "render/MetalGpuDiffusePathLoopBackend.h"
#endif
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanGpuDiffusePathFrontierCompactionBackend.h"
#include "render/VulkanGpuDiffusePathLoopBackend.h"
#endif

#include <stdexcept>
#include <utility>

namespace render {
  namespace {
    constexpr const char* kNoPlatformPathLoopReason =
      "platform full-GPU path-loop kernel is not available yet";
  }

  std::shared_ptr<const GpuDiffusePathLoopBackend>
  GpuDiffusePathLoopBackend::defaultBackendForGpuRequest() {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    {
      auto compactionBackend = std::make_shared<MetalGpuDiffusePathFrontierCompactionBackend>();
      if (compactionBackend->compactionPathAvailable()) {
        return std::make_shared<CompactingGpuDiffusePathLoopBackend>(compactionBackend);
      }
    }
#endif
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    {
      auto compactionBackend = std::make_shared<VulkanGpuDiffusePathFrontierCompactionBackend>();
      if (compactionBackend->compactionPathAvailable()) {
        return std::make_shared<CompactingGpuDiffusePathLoopBackend>(compactionBackend);
      }
    }
#endif
    return CpuReferenceGpuDiffusePathLoopBackend::sharedInstance();
  }

  std::shared_ptr<const GpuDiffusePathLoopBackend>
  GpuDiffusePathLoopBackend::defaultFullGpuBackendForGpuRequest() {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    return MetalGpuDiffusePathLoopBackend::sharedInstance();
#elif defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanGpuDiffusePathLoopBackend::sharedInstance();
#else
    return {};
#endif
  }

  bool GpuDiffusePathLoopBackend::fullGpuPathLoopAvailable() const {
    return false;
  }

  const char* GpuDiffusePathLoopBackend::fullGpuPathLoopUnavailableReason() const {
    return kNoPlatformPathLoopReason;
  }

  const char* GpuDiffusePathLoopBackend::platformName() const {
    return "";
  }

  GpuDiffusePathLoopBackendSupport
  GpuDiffusePathLoopBackend::fullGpuPathLoopSupport(const GpuTracingSceneSections& scene) const {
    return fullGpuPathLoopSupport(scene, GpuDiffusePathLoopSettings());
  }

  GpuDiffusePathLoopBackendSupport
  GpuDiffusePathLoopBackend::fullGpuPathLoopSupport(const GpuTracingSceneSections&,
                                                    const GpuDiffusePathLoopSettings&) const {
    if (!fullGpuPathLoopAvailable()) {
      return {false, fullGpuPathLoopUnavailableReason()};
    }
    return {true, {}};
  }

  std::shared_ptr<const CpuReferenceGpuDiffusePathLoopBackend>
  CpuReferenceGpuDiffusePathLoopBackend::sharedInstance() {
    static const std::shared_ptr<const CpuReferenceGpuDiffusePathLoopBackend> instance =
      std::make_shared<CpuReferenceGpuDiffusePathLoopBackend>();
    return instance;
  }

  const char* CpuReferenceGpuDiffusePathLoopBackend::name() const {
    return "compiled_cpu_reference";
  }

  GpuDiffusePathLoopResult CpuReferenceGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const GpuDiffusePathLoopSettings& settings) const {
    return GpuDiffusePathLoop().run(scene, initialPathStates, settings);
  }

  CompactingGpuDiffusePathLoopBackend::CompactingGpuDiffusePathLoopBackend(
    std::shared_ptr<const GpuDiffusePathFrontierCompactionBackend> compactionBackend)
      : m_compactionBackend(std::move(compactionBackend)) {
    if (!m_compactionBackend) {
      throw std::invalid_argument("compacting GPU diffuse path-loop backend requires compaction");
    }
  }

  const char* CompactingGpuDiffusePathLoopBackend::name() const {
    return "compiled_cpu_reference_with_compaction_backend";
  }

  const GpuDiffusePathFrontierCompactionBackend&
  CompactingGpuDiffusePathLoopBackend::compactionBackend() const {
    return *m_compactionBackend;
  }

  GpuDiffusePathLoopResult CompactingGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const GpuDiffusePathLoopSettings& settings) const {
    return GpuDiffusePathLoop().run(scene, initialPathStates, settings, *m_compactionBackend);
  }
}
