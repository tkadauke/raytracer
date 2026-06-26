#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "render/GpuIntersectionScene.h"
#include "render/GpuTracingScene.h"

namespace render {
  class GpuTracingBsdfEvaluator {
  public:
    explicit GpuTracingBsdfEvaluator(const GpuTracingSceneSections& scene);

    [[nodiscard]] static bool isFiniteSurfaceMaterial(GpuTracingMaterialKind kind);
    [[nodiscard]] static bool hasFinitePhongLobes(GpuTracingMaterialKind kind);
    [[nodiscard]] static double diffuseSamplingWeight(const GpuTracingMaterialRecord& material);
    [[nodiscard]] static double finiteBsdfPdf(const GpuTracingMaterialRecord& material,
                                              const Vector3d& normal, const Vector3d& wi,
                                              const Vector3d& wo);

    [[nodiscard]] Colord diffuseBsdf(const GpuTracingMaterialRecord& material,
                                     const GpuIntersectionHitRecord& hit) const;
    [[nodiscard]] Colord finiteBsdf(const GpuTracingMaterialRecord& material,
                                    const GpuIntersectionHitRecord& hit, const Vector3d& normal,
                                    const Vector3d& wi, const Vector3d& wo) const;

  private:
    [[nodiscard]] static double cosineHemispherePdf(const Vector3d& normal,
                                                    const Vector3d& direction);
    [[nodiscard]] static Colord glossyPhongBsdf(const GpuTracingMaterialRecord& material,
                                                const Vector3d& normal, const Vector3d& wi,
                                                const Vector3d& wo);
    [[nodiscard]] static double phongLobePdf(const GpuTracingMaterialRecord& material,
                                             const Vector3d& normal, const Vector3d& wi,
                                             const Vector3d& wo);

    const GpuTracingSceneSections& m_scene;
  };
}
