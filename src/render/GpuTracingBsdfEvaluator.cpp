#include "render/GpuTracingBsdfEvaluator.h"

#include "core/math/Constants.h"
#include "render/GpuTracingTextureEvaluator.h"

#include <algorithm>
#include <cmath>

namespace render {
  GpuTracingBsdfEvaluator::GpuTracingBsdfEvaluator(const GpuTracingSceneSections& scene)
      : m_scene(scene) {
  }

  bool GpuTracingBsdfEvaluator::isFiniteSurfaceMaterial(GpuTracingMaterialKind kind) {
    return kind == GpuTracingMaterialKind::Matte || kind == GpuTracingMaterialKind::Phong ||
           kind == GpuTracingMaterialKind::Reflective ||
           kind == GpuTracingMaterialKind::Transparent;
  }

  bool GpuTracingBsdfEvaluator::hasFinitePhongLobes(GpuTracingMaterialKind kind) {
    return kind == GpuTracingMaterialKind::Phong || kind == GpuTracingMaterialKind::Reflective ||
           kind == GpuTracingMaterialKind::Transparent;
  }

  double GpuTracingBsdfEvaluator::diffuseSamplingWeight(const GpuTracingMaterialRecord& material) {
    const double diffuse = std::max(0.0f, material.parameters[1]);
    const double specular = std::max(0.0f, material.parameters[2]);
    const double total = diffuse + specular;
    return total <= 0.0 ? 1.0 : diffuse / total;
  }

  double GpuTracingBsdfEvaluator::cosineHemispherePdf(const Vector3d& normal,
                                                      const Vector3d& direction) {
    const double normalDotDirection = normal * direction;
    return normalDotDirection <= 0.0 ? 0.0 : normalDotDirection * invPI;
  }

  Colord GpuTracingBsdfEvaluator::diffuseBsdf(const GpuTracingMaterialRecord& material,
                                              const GpuIntersectionHitRecord& hit) const {
    return GpuTracingTextureEvaluator(m_scene).evaluate(material.albedoTexture, hit) *
           material.parameters[1] * invPI;
  }

  Colord GpuTracingBsdfEvaluator::glossyPhongBsdf(const GpuTracingMaterialRecord& material,
                                                  const Vector3d& normal, const Vector3d& wi,
                                                  const Vector3d& wo) {
    if (normal * wi < 0.0 || normal * wo < 0.0) {
      return Colord::black();
    }
    const Vector3d lobeAxis = (-wi).reflect(normal).normalized();
    const double lobeDotOut = lobeAxis * wo.normalized();
    if (lobeDotOut <= 0.0) {
      return Colord::black();
    }
    return Colord(material.specularParameters) * material.parameters[2] *
           std::pow(lobeDotOut, material.parameters[3]);
  }

  Colord GpuTracingBsdfEvaluator::finiteBsdf(const GpuTracingMaterialRecord& material,
                                             const GpuIntersectionHitRecord& hit,
                                             const Vector3d& normal, const Vector3d& wi,
                                             const Vector3d& wo) const {
    Colord value = diffuseBsdf(material, hit);
    if (hasFinitePhongLobes(static_cast<GpuTracingMaterialKind>(material.kind))) {
      value += glossyPhongBsdf(material, normal, wi, wo);
    }
    return value;
  }

  double GpuTracingBsdfEvaluator::phongLobePdf(const GpuTracingMaterialRecord& material,
                                               const Vector3d& normal, const Vector3d& wi,
                                               const Vector3d& wo) {
    if (normal * wi < 0.0 || normal * wo < 0.0) {
      return 0.0;
    }
    const Vector3d lobeAxis = (-wi).reflect(normal).normalized();
    const double lobeDotOut = lobeAxis * wo.normalized();
    if (lobeDotOut <= 0.0) {
      return 0.0;
    }
    return ((material.parameters[3] + 1.0) * invTAU) * std::pow(lobeDotOut, material.parameters[3]);
  }

  double GpuTracingBsdfEvaluator::finiteBsdfPdf(const GpuTracingMaterialRecord& material,
                                                const Vector3d& normal, const Vector3d& wi,
                                                const Vector3d& wo) {
    const auto kind = static_cast<GpuTracingMaterialKind>(material.kind);
    if (kind == GpuTracingMaterialKind::Matte) {
      return cosineHemispherePdf(normal, wo);
    }
    if (kind == GpuTracingMaterialKind::Phong) {
      const double diffuseWeight = diffuseSamplingWeight(material);
      const double specularWeight = 1.0 - diffuseWeight;
      return diffuseWeight * cosineHemispherePdf(normal, wo) +
             specularWeight * phongLobePdf(material, normal, wi, wo);
    }
    return 0.0;
  }
}
