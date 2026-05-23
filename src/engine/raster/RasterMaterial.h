#pragma once

#include "core/Color.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"
#include "engine/raster/Rasterizer.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/Material.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Primitive.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/Texture.h"
#include "render/textures/UVColorTexture.h"
#include "render/textures/mappings/UVMapping2D.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <typeinfo>
#include <utility>

namespace engine::raster::detail {

  // Stable diagnostic color for primitives without a material the rasterizer can
  // interpret. The hash keeps missing-material output visible without requiring
  // global state or per-run randomness.
  inline Colord fallbackFaceColor(std::uint64_t index) {
    const std::uint64_t r = (index * 2654435761ULL) & 0xFFu;
    const std::uint64_t g = (index * 40503ULL + 12345) & 0xFFu;
    const std::uint64_t b = (index * 15485863ULL + 999983) & 0xFFu;
    return Colord(0.3 + (r / 255.0) * 0.7, 0.3 + (g / 255.0) * 0.7, 0.3 + (b / 255.0) * 0.7);
  }

  inline double rasterAlphaFromTextureColor(const Colord& color) {
    return std::clamp(color.max(), 0.0, 1.0);
  }

  // Raster-native wrapper around render textures. Textures that only need the
  // interpolated UV can be evaluated directly; arbitrary textures still fall
  // back to the render::Texture interface with a synthetic ray-hit context.
  class RasterTexture {
  public:
    RasterTexture()
        : m_kind(Kind::Constant),
          m_color(Colord::black()),
          m_uScale(1.0),
          m_vScale(1.0) {
    }

    static RasterTexture constant(const Colord& color) {
      RasterTexture result;
      result.m_color = color;
      return result;
    }

    static RasterTexture from(std::shared_ptr<render::Texturec> texture) {
      if (!texture)
        return fallback(nullptr);

      const render::Texturec* texturePtr = texture.get();
      if (typeid(*texturePtr) == typeid(render::ConstantColorTexture)) {
        const auto* constantTexture = static_cast<const render::ConstantColorTexture*>(texturePtr);
        return constant(constantTexture->color());
      }

      if (typeid(*texturePtr) == typeid(render::UVColorTexture)) {
        RasterTexture result;
        result.m_kind = Kind::UVColor;
        return result;
      }

      if (typeid(*texturePtr) == typeid(render::ImageTexture)) {
        const auto* image = static_cast<const render::ImageTexture*>(texturePtr);
        const render::TextureMapping2D* mapping = image->mapping();
        if (mapping && typeid(*mapping) == typeid(render::UVMapping2D)) {
          const auto* uvMapping = static_cast<const render::UVMapping2D*>(mapping);
          RasterTexture result;
          result.m_kind = Kind::Image;
          result.m_texture = texture;
          result.m_image = image;
          result.m_uScale = uvMapping->uScale();
          result.m_vScale = uvMapping->vScale();
          return result;
        }
      }

      if (typeid(*texturePtr) == typeid(render::CheckerBoardTexture)) {
        const auto* checker = static_cast<const render::CheckerBoardTexture*>(texturePtr);
        const render::TextureMapping2D* mapping = checker->mapping();
        if (mapping && typeid(*mapping) == typeid(render::UVMapping2D) &&
            checker->brightTexture() && checker->darkTexture()) {
          const auto* uvMapping = static_cast<const render::UVMapping2D*>(mapping);
          RasterTexture result;
          result.m_kind = Kind::UVChecker;
          result.m_uScale = uvMapping->uScale();
          result.m_vScale = uvMapping->vScale();
          result.m_bright = std::make_shared<RasterTexture>(from(checker->brightTexture()));
          result.m_dark = std::make_shared<RasterTexture>(from(checker->darkTexture()));
          return result;
        }
      }

      return fallback(std::move(texture));
    }

    Colord evaluate(const render::Primitive* primitive, const Vector3d& worldPos,
                    const Vector3d& normal, const Vector2d& uv,
                    const Vector2d& uvDx = Vector2d::null,
                    const Vector2d& uvDy = Vector2d::null) const {
      switch (m_kind) {
      case Kind::Constant:
        return m_color;
      case Kind::UVColor:
        return Colord(uv.x(), uv.y(), 0.0);
      case Kind::Image:
        return m_image->sample(uv.x() * m_uScale, uv.y() * m_vScale,
                               Vector2d(uvDx.x() * m_uScale, uvDx.y() * m_vScale),
                               Vector2d(uvDy.x() * m_uScale, uvDy.y() * m_vScale));
      case Kind::UVChecker:
        return checkerChild(uv).evaluate(primitive, worldPos, normal, uv, uvDx, uvDy);
      case Kind::Fallback: {
        const HitPoint hp(primitive, 0.0, Vector4d(worldPos), normal, uv);
        const Rayd ray(worldPos, -normal);
        return m_texture->evaluate(ray, hp);
      }
      }
      return m_color;
    }

  private:
    enum class Kind { Constant, UVColor, UVChecker, Image, Fallback };

    static RasterTexture fallback(std::shared_ptr<render::Texturec> texture) {
      RasterTexture result;
      result.m_kind = Kind::Fallback;
      result.m_texture = std::move(texture);
      return result;
    }

    const RasterTexture& checkerChild(const Vector2d& uv) const {
      const double s = uv.x() * m_uScale;
      const double t = uv.y() * m_vScale;
      const int parity = static_cast<int>(std::floor(s)) + static_cast<int>(std::floor(t));
      return (parity % 2 == 0) ? *m_bright : *m_dark;
    }

    Kind m_kind;
    Colord m_color;
    std::shared_ptr<render::Texturec> m_texture;
    const render::ImageTexture* m_image = nullptr;
    double m_uScale;
    double m_vScale;
    std::shared_ptr<RasterTexture> m_bright;
    std::shared_ptr<RasterTexture> m_dark;
  };

  // Per-triangle material adapter used by the built-in fragment path. Most
  // triangles collapse to a constant albedo; texture-backed materials keep a
  // raster texture adapter and evaluate it with the interpolated fragment
  // context only when a fragment is actually shaded. Common UV-only textures
  // avoid constructing ray-hit context in the fragment loop.
  class RasterMaterial {
  public:
    RasterMaterial()
        : m_albedo(RasterTexture::constant(Colord::black())),
          m_ambientCoefficient(1.0),
          m_diffuseCoefficient(1.0),
          m_materialAlpha(1.0),
          m_specularColor(Colord::black()),
          m_specularCoefficient(0.0),
          m_specularExponent(16.0) {
    }

    static RasterMaterial constant(const Colord& albedo, double ambientCoefficient = 1.0,
                                   double diffuseCoefficient = 1.0, double materialAlpha = 1.0,
                                   const Colord& specularColor = Colord::black(),
                                   double specularCoefficient = 0.0,
                                   double specularExponent = 16.0) {
      return RasterMaterial(RasterTexture::constant(albedo), ambientCoefficient, diffuseCoefficient,
                            materialAlpha, specularColor, specularCoefficient, specularExponent);
    }

    static RasterMaterial texture(const RasterTexture& texture, double ambientCoefficient,
                                  double diffuseCoefficient, double materialAlpha,
                                  const Colord& specularColor, double specularCoefficient,
                                  double specularExponent) {
      return RasterMaterial(texture, ambientCoefficient, diffuseCoefficient, materialAlpha,
                            specularColor, specularCoefficient, specularExponent);
    }

    static RasterMaterial texture(std::shared_ptr<render::Texturec> texture,
                                  double ambientCoefficient, double diffuseCoefficient,
                                  double materialAlpha,
                                  const Colord& specularColor, double specularCoefficient,
                                  double specularExponent) {
      return RasterMaterial(RasterTexture::from(std::move(texture)), ambientCoefficient,
                            diffuseCoefficient, materialAlpha, specularColor, specularCoefficient,
                            specularExponent);
    }

    Colord albedo(const render::Primitive* primitive, const Vector3d& worldPos,
                  const Vector3d& normal, const Vector2d& uv, const Vector2d& uvDx,
                  const Vector2d& uvDy) const {
      return m_albedo.evaluate(primitive, worldPos, normal, uv, uvDx, uvDy);
    }

    double alpha(const render::Primitive* primitive, const Vector3d& worldPos,
                 const Vector3d& normal, const Vector2d& uv, const Vector2d& uvDx,
                 const Vector2d& uvDy) const {
      const Colord textureColor = m_albedo.evaluate(primitive, worldPos, normal, uv, uvDx, uvDy);
      return std::clamp(m_materialAlpha * rasterAlphaFromTextureColor(textureColor), 0.0, 1.0);
    }

    double ambientCoefficient() const {
      return m_ambientCoefficient;
    }

    double diffuseCoefficient() const {
      return m_diffuseCoefficient;
    }

    bool hasSpecular() const {
      return m_specularCoefficient > 0.0 && m_specularColor.max() > 0.0;
    }

    const Colord& specularColor() const {
      return m_specularColor;
    }

    double specularCoefficient() const {
      return m_specularCoefficient;
    }

    double specularExponent() const {
      return m_specularExponent;
    }

  private:
    RasterMaterial(const RasterTexture& albedo, double ambientCoefficient,
                   double diffuseCoefficient, double materialAlpha, const Colord& specularColor,
                   double specularCoefficient, double specularExponent)
        : m_albedo(albedo),
          m_ambientCoefficient(ambientCoefficient),
          m_diffuseCoefficient(diffuseCoefficient),
          m_materialAlpha(std::clamp(materialAlpha, 0.0, 1.0)),
          m_specularColor(specularColor),
          m_specularCoefficient(specularCoefficient),
          m_specularExponent(specularExponent) {
    }

    RasterTexture m_albedo;
    double m_ambientCoefficient;
    double m_diffuseCoefficient;
    double m_materialAlpha;
    Colord m_specularColor;
    double m_specularCoefficient;
    double m_specularExponent;
  };

  // Per-leaf material classifier. The emitter builds one source per primitive
  // leaf, then asks it for a concrete RasterMaterial for each emitted face so
  // expensive material/type checks stay out of the fragment loop.
  class RasterMaterialSource {
  public:
    static RasterMaterialSource from(const std::shared_ptr<render::Material>& material) {
      const auto sidedness =
        material ? material->sidedness() : render::Material::Sidedness::TwoSided;
      auto matte = std::dynamic_pointer_cast<render::MatteMaterial>(material);
      if (!matte)
        return faceColor(sidedness);

      const auto phong = std::dynamic_pointer_cast<render::PhongMaterial>(material);
      auto texture = matte->diffuseTexture();
      if (!texture)
        return faceColor(sidedness);

      const render::Texturec* texturePtr = texture.get();
      if (typeid(*texturePtr) == typeid(render::ConstantColorTexture)) {
        const auto* constant = static_cast<const render::ConstantColorTexture*>(texturePtr);
        return constantAlbedo(constant->color(), *matte, phong.get(), sidedness);
      }

      return textured(RasterTexture::from(std::move(texture)), *matte, phong.get(), sidedness);
    }

    Rasterizer::CullMode defaultCullMode() const {
      switch (m_sidedness) {
      case render::Material::Sidedness::Front:
        return Rasterizer::CullMode::Back;
      case render::Material::Sidedness::Back:
        return Rasterizer::CullMode::Front;
      case render::Material::Sidedness::TwoSided:
        return Rasterizer::CullMode::Both;
      }
      return Rasterizer::CullMode::Both;
    }

    RasterMaterial forFace(std::uint64_t faceIdx) const {
      switch (m_kind) {
      case Kind::FaceColor:
        return RasterMaterial::constant(fallbackFaceColor(faceIdx));
      case Kind::Constant:
        return RasterMaterial::constant(m_albedo, m_ambientCoefficient, m_diffuseCoefficient,
                                        m_materialAlpha, m_specularColor, m_specularCoefficient,
                                        m_specularExponent);
      case Kind::Texture:
        return RasterMaterial::texture(m_texture, m_ambientCoefficient, m_diffuseCoefficient,
                                       m_materialAlpha, m_specularColor, m_specularCoefficient,
                                       m_specularExponent);
      }
      return RasterMaterial::constant(fallbackFaceColor(faceIdx));
    }

  private:
    enum class Kind { FaceColor, Constant, Texture };

    static RasterMaterialSource faceColor(render::Material::Sidedness sidedness) {
      return RasterMaterialSource(Kind::FaceColor, Colord::black(),
                                  RasterTexture::constant(Colord::black()), sidedness);
    }

    static RasterMaterialSource constantAlbedo(const Colord& albedo,
                                               const render::MatteMaterial& matte,
                                               const render::PhongMaterial* phong,
                                               render::Material::Sidedness sidedness) {
      return material(Kind::Constant, albedo, RasterTexture::constant(Colord::black()), matte,
                      phong, sidedness);
    }

    static RasterMaterialSource textured(const RasterTexture& texture,
                                         const render::MatteMaterial& matte,
                                         const render::PhongMaterial* phong,
                                         render::Material::Sidedness sidedness) {
      return material(Kind::Texture, Colord::black(), texture, matte, phong, sidedness);
    }

    static RasterMaterialSource material(Kind kind, const Colord& albedo,
                                         const RasterTexture& texture,
                                         const render::MatteMaterial& matte,
                                         const render::PhongMaterial* phong,
                                         render::Material::Sidedness sidedness) {
      const Colord specularColor = phong ? phong->specularColor() : Colord::black();
      const double specularCoefficient = phong ? phong->specularCoefficient() : 0.0;
      const double specularExponent = phong ? phong->exponent() : 16.0;
      const auto* transparent = dynamic_cast<const render::TransparentMaterial*>(&matte);
      const double materialAlpha = transparent ? 1.0 - transparent->transmissionCoefficient() : 1.0;
      return RasterMaterialSource(kind, albedo, texture, sidedness, matte.ambientCoefficient(),
                                  matte.diffuseCoefficient(), materialAlpha, specularColor,
                                  specularCoefficient, specularExponent);
    }

    RasterMaterialSource(Kind kind, const Colord& albedo, const RasterTexture& texture,
                         render::Material::Sidedness sidedness,
                         double ambientCoefficient = 1.0, double diffuseCoefficient = 1.0,
                         double materialAlpha = 1.0,
                         const Colord& specularColor = Colord::black(),
                         double specularCoefficient = 0.0, double specularExponent = 16.0)
        : m_kind(kind),
          m_albedo(albedo),
          m_texture(texture),
          m_sidedness(sidedness),
          m_ambientCoefficient(ambientCoefficient),
          m_diffuseCoefficient(diffuseCoefficient),
          m_materialAlpha(std::clamp(materialAlpha, 0.0, 1.0)),
          m_specularColor(specularColor),
          m_specularCoefficient(specularCoefficient),
          m_specularExponent(specularExponent) {
    }

    Kind m_kind;
    Colord m_albedo;
    RasterTexture m_texture;
    render::Material::Sidedness m_sidedness;
    double m_ambientCoefficient;
    double m_diffuseCoefficient;
    double m_materialAlpha;
    Colord m_specularColor;
    double m_specularCoefficient;
    double m_specularExponent;
  };

}
