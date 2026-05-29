#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "engine/raster/Rasterizer.h"
#include "render/materials/Material.h"
#include "render/textures/Texture.h"

#include <cstdint>
#include <memory>

namespace render {
  class ImageTexture;
  class MatteMaterial;
  class PhongMaterial;
  class Primitive;
}

namespace engine::raster::detail {

  enum class RasterAlbedoShaderMode {
    VertexColor = 0,
    UVColor = 1,
    ImageTexture = 2,
    UVChecker = 3
  };

  struct RasterAlbedoShaderSource {
    RasterAlbedoShaderMode mode{RasterAlbedoShaderMode::VertexColor};
    std::shared_ptr<render::Texturec> texture;
    const render::ImageTexture* image{nullptr};
    double uScale{1.0};
    double vScale{1.0};
    Colord checkerBright{Colord::white()};
    Colord checkerDark{Colord::black()};
    Colord tint{Colord::white()};

    bool operator==(const RasterAlbedoShaderSource& other) const;
    bool operator!=(const RasterAlbedoShaderSource& other) const;
  };

  struct RasterTangentFrame {
    Vector3d tangent{Vector3d::undefined};
    Vector3d bitangent{Vector3d::undefined};
    bool available{false};
  };

  // Raster-native wrapper around render textures. Textures that only need the
  // interpolated UV can be evaluated directly; arbitrary textures still fall
  // back to the render::Texture interface with a synthetic ray-hit context.
  class RasterTexture {
  public:
    RasterTexture();

    static RasterTexture constant(const Colord& color);

    static RasterTexture from(std::shared_ptr<render::Texturec> texture);

    Colord evaluate(const render::Primitive* primitive, const Vector3d& worldPos,
                    const Vector3d& normal, const Vector2d& uv,
                    const Vector2d& uvDx = Vector2d::null,
                    const Vector2d& uvDy = Vector2d::null) const;

    RasterAlbedoShaderSource shaderAlbedoSource() const;

  private:
    enum class Kind { Constant, UVColor, UVChecker, Image, Fallback };

    static RasterTexture fallback(std::shared_ptr<render::Texturec> texture);

    const RasterTexture& checkerChild(const Vector2d& uv) const;

    Kind m_kind;
    Colord m_color;
    Colord m_tint;
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
    RasterMaterial();

    static RasterMaterial
    constant(const Colord& albedo, double ambientCoefficient = 1.0, double diffuseCoefficient = 1.0,
             double materialAlpha = 1.0, const Colord& specularColor = Colord::black(),
             double specularCoefficient = 0.0, double specularExponent = 16.0,
             const RasterTexture& normalMap = RasterTexture::constant(Colord(0.5, 0.5, 1.0)),
             bool hasNormalMap = false);

    static RasterMaterial
    texture(const RasterTexture& texture, double ambientCoefficient, double diffuseCoefficient,
            double materialAlpha, const Colord& specularColor, double specularCoefficient,
            double specularExponent,
            const RasterTexture& normalMap = RasterTexture::constant(Colord(0.5, 0.5, 1.0)),
            bool hasNormalMap = false);

    static RasterMaterial texture(std::shared_ptr<render::Texturec> texture,
                                  double ambientCoefficient, double diffuseCoefficient,
                                  double materialAlpha, const Colord& specularColor,
                                  double specularCoefficient, double specularExponent,
                                  std::shared_ptr<render::Texturec> normalMap = nullptr);

    Colord albedo(const render::Primitive* primitive, const Vector3d& worldPos,
                  const Vector3d& normal, const Vector2d& uv, const Vector2d& uvDx,
                  const Vector2d& uvDy) const;

    double alpha(const render::Primitive* primitive, const Vector3d& worldPos,
                 const Vector3d& normal, const Vector2d& uv, const Vector2d& uvDx,
                 const Vector2d& uvDy) const;

    bool hasNormalMap() const;

    RasterAlbedoShaderSource shaderAlbedoSource() const;

    double materialAlpha() const;

    Vector3d lightingNormal(const render::Primitive* primitive, const Vector3d& worldPos,
                            const Vector3d& normal, const Vector2d& uv, const Vector2d& uvDx,
                            const Vector2d& uvDy, const RasterTangentFrame& tangentFrame) const;

    double ambientCoefficient() const;

    double diffuseCoefficient() const;

    bool hasSpecular() const;

    const Colord& specularColor() const;

    double specularCoefficient() const;

    double specularExponent() const;

  private:
    RasterMaterial(const RasterTexture& albedo, double ambientCoefficient,
                   double diffuseCoefficient, double materialAlpha, const Colord& specularColor,
                   double specularCoefficient, double specularExponent,
                   const RasterTexture& normalMap, bool hasNormalMap);

    RasterTexture m_albedo;
    RasterTexture m_normalMap;
    bool m_hasNormalMap;
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
    using RecursiveFallback = render::Material::RasterRecursiveFallback;

    static RasterMaterialSource from(const std::shared_ptr<render::Material>& material);

    Rasterizer::CullMode defaultCullMode() const;

    RasterMaterial forFace(std::uint64_t faceIdx) const;

    RasterMaterialSource withColorOverride(const Colord& albedo) const;

    RecursiveFallback recursiveFallback() const;

    bool usesRecursiveFallback() const;

    const char* recursiveFallbackName() const;

  private:
    enum class Kind { FaceColor, Constant, Texture };

    static RecursiveFallback recursiveFallbackFor(const render::Material* material);

    static RasterMaterialSource faceColor(render::Material::Sidedness sidedness,
                                          RecursiveFallback recursiveFallback);

    static RasterMaterialSource constantAlbedo(const Colord& albedo,
                                               const render::MatteMaterial& matte,
                                               const render::PhongMaterial* phong,
                                               render::Material::Sidedness sidedness,
                                               RecursiveFallback recursiveFallback);

    static RasterMaterialSource textured(const RasterTexture& texture,
                                         const render::MatteMaterial& matte,
                                         const render::PhongMaterial* phong,
                                         render::Material::Sidedness sidedness,
                                         RecursiveFallback recursiveFallback);

    static RasterMaterialSource
    material(Kind kind, const Colord& albedo, const RasterTexture& texture,
             const render::MatteMaterial& matte, const render::PhongMaterial* phong,
             render::Material::Sidedness sidedness, RecursiveFallback recursiveFallback);

    RasterMaterialSource(Kind kind, const Colord& albedo, const RasterTexture& texture,
                         render::Material::Sidedness sidedness,
                         RecursiveFallback recursiveFallback = RecursiveFallback::None,
                         double ambientCoefficient = 1.0, double diffuseCoefficient = 1.0,
                         double materialAlpha = 1.0, const Colord& specularColor = Colord::black(),
                         double specularCoefficient = 0.0, double specularExponent = 16.0,
                         const RasterTexture& normalMap = RasterTexture::constant(Colord(0.5, 0.5,
                                                                                         1.0)),
                         bool hasNormalMap = false);

    Kind m_kind;
    Colord m_albedo;
    RasterTexture m_texture;
    RasterTexture m_normalMap;
    bool m_hasNormalMap;
    render::Material::Sidedness m_sidedness;
    RecursiveFallback m_recursiveFallback;
    double m_ambientCoefficient;
    double m_diffuseCoefficient;
    double m_materialAlpha;
    Colord m_specularColor;
    double m_specularCoefficient;
    double m_specularExponent;
  };

}
