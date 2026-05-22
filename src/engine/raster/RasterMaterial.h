#pragma once

#include "core/Color.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Primitive.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/Texture.h"

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

  // Per-triangle material adapter used by the built-in fragment path. Most
  // triangles collapse to a constant albedo; texture-backed materials keep a
  // shared texture pointer and evaluate it with the interpolated fragment
  // context only when a fragment is actually shaded.
  class RasterMaterial {
  public:
    RasterMaterial()
        : m_kind(Kind::Constant),
          m_albedo(Colord::black()),
          m_texture(nullptr) {
    }

    static RasterMaterial constant(const Colord& albedo) {
      return RasterMaterial(Kind::Constant, albedo, nullptr);
    }

    static RasterMaterial texture(std::shared_ptr<render::Texturec> texture) {
      return RasterMaterial(Kind::Texture, Colord::black(), std::move(texture));
    }

    Colord albedo(const render::Primitive* primitive, const Vector3d& worldPos,
                  const Vector3d& normal, const Vector2d& uv) const {
      if (m_kind == Kind::Constant || !m_texture)
        return m_albedo;

      const HitPoint hp(primitive, 0.0, Vector4d(worldPos), normal, uv);
      const Rayd ray(worldPos, -normal);
      return m_texture->evaluate(ray, hp);
    }

  private:
    enum class Kind { Constant, Texture };

    RasterMaterial(Kind kind, const Colord& albedo, std::shared_ptr<render::Texturec> texture)
        : m_kind(kind),
          m_albedo(albedo),
          m_texture(std::move(texture)) {
    }

    Kind m_kind;
    Colord m_albedo;
    std::shared_ptr<render::Texturec> m_texture;
  };

  // Per-leaf material classifier. The emitter builds one source per primitive
  // leaf, then asks it for a concrete RasterMaterial for each emitted face so
  // expensive material/type checks stay out of the fragment loop.
  class RasterMaterialSource {
  public:
    static RasterMaterialSource from(const std::shared_ptr<render::Material>& material) {
      auto matte = std::dynamic_pointer_cast<render::MatteMaterial>(material);
      if (!matte)
        return faceColor();

      auto texture = matte->diffuseTexture();
      if (!texture)
        return faceColor();

      const render::Texturec* texturePtr = texture.get();
      if (typeid(*texturePtr) == typeid(render::ConstantColorTexture)) {
        const auto* constant = static_cast<const render::ConstantColorTexture*>(texturePtr);
        return constantAlbedo(constant->color());
      }

      return textured(std::move(texture));
    }

    RasterMaterial forFace(std::uint64_t faceIdx) const {
      switch (m_kind) {
      case Kind::FaceColor:
        return RasterMaterial::constant(fallbackFaceColor(faceIdx));
      case Kind::Constant:
        return RasterMaterial::constant(m_albedo);
      case Kind::Texture:
        return RasterMaterial::texture(m_texture);
      }
      return RasterMaterial::constant(fallbackFaceColor(faceIdx));
    }

  private:
    enum class Kind { FaceColor, Constant, Texture };

    static RasterMaterialSource faceColor() {
      return RasterMaterialSource(Kind::FaceColor, Colord::black(), nullptr);
    }

    static RasterMaterialSource constantAlbedo(const Colord& albedo) {
      return RasterMaterialSource(Kind::Constant, albedo, nullptr);
    }

    static RasterMaterialSource textured(std::shared_ptr<render::Texturec> texture) {
      return RasterMaterialSource(Kind::Texture, Colord::black(), std::move(texture));
    }

    RasterMaterialSource(Kind kind, const Colord& albedo, std::shared_ptr<render::Texturec> texture)
        : m_kind(kind),
          m_albedo(albedo),
          m_texture(std::move(texture)) {
    }

    Kind m_kind;
    Colord m_albedo;
    std::shared_ptr<render::Texturec> m_texture;
  };

}
