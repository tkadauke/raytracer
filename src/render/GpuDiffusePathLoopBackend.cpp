#include "render/GpuDiffusePathLoopBackend.h"

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
}
