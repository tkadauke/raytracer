#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "render/GpuDirectLightWork.h"
#include "render/GpuTracingScene.h"

#include <cstdint>

namespace render {
  enum class GpuCompiledLightSampleStatus : std::uint32_t {
    Valid = 0,
    UnsupportedLight = 1,
    DegenerateLight = 2,
    CoincidentPoint = 3,
    BackFacing = 4
  };

  struct GpuCompiledLightSelection {
    bool valid{false};
    std::uint32_t lightIndex{0};
    double pdf{0.0};
  };

  struct GpuCompiledLightSample {
    GpuCompiledLightSampleStatus status{GpuCompiledLightSampleStatus::UnsupportedLight};
    Vector3d direction{Vector3d::null};
    Colord radiance{Colord::black()};
    double distance{0.0};
    double pdf{0.0};
    bool delta{false};
    Vector2d surfaceSample{0.5, 0.5};

    [[nodiscard]] bool valid() const {
      return status == GpuCompiledLightSampleStatus::Valid && pdf > 0.0;
    }
  };

  [[nodiscard]] double gpuCompiledLightSelectionWeight(const GpuTracingLightRecord& light);

  [[nodiscard]] GpuCompiledLightSelection
  selectGpuCompiledLight(const GpuTracingSceneSections& scene,
                         const GpuDirectLightSelectionRecord& selection, double unitSample);

  [[nodiscard]] GpuCompiledLightSample
  sampleGpuCompiledLight(const GpuTracingLightRecord& light, const Vector3d& point,
                         const Vector2d& lightSample = Vector2d(0.5, 0.5));

  [[nodiscard]] double gpuCompiledLightPdf(const GpuTracingLightRecord& light,
                                           const Vector3d& point, const Vector3d& direction);
}
