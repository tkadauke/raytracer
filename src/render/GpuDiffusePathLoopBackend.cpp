#include "render/GpuDiffusePathLoopBackend.h"

#include <stdexcept>
#include <utility>

namespace render {
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
