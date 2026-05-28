#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "render/textures/Texture.h"

#include <memory>
#include <string>
#include <vector>

namespace render {
  class TextureMapping2D;

  enum class ImageTextureFilter { Nearest, Bilinear, Mipmap };

  enum class ImageTextureWrap { Clamp, Repeat };

  /**
    * Image-backed color texture with explicit CPU filtering.
    *
    * `Nearest` preserves the historical point-sampled look, `Bilinear`
    * blends the four surrounding texels in the base image, and `Mipmap`
    * generates a CPU mip chain and chooses/blends levels from UV derivatives
    * when the caller supplies them. Plain Texture::evaluate() has no
    * derivative channel, so mipmapped textures use level 0 from that path; the
    * rasterizer fast path supplies screen-space UV gradients.
    *
    * @image html image_texture_filter_nearest.png "Nearest image texture sampling"
    *
    * <table><tr>
    * <td>@image html image_texture_filter_nearest.png "nearest"</td>
    * <td>@image html image_texture_filter_bilinear.png "bilinear"</td>
    * <td>@image html image_texture_filter_mipmap.png "mipmap"</td>
    * </tr></table>
    */
  class ImageTexture : public Texturec {
  public:
    ImageTexture(TextureMapping2D* mapping, int width, int height,
                 const std::vector<Colord>& pixels,
                 ImageTextureFilter filter = ImageTextureFilter::Nearest,
                 ImageTextureWrap wrap = ImageTextureWrap::Repeat);
    virtual ~ImageTexture();

    static std::shared_ptr<ImageTexture>
    fromFile(TextureMapping2D* mapping, const std::string& path,
             ImageTextureFilter filter = ImageTextureFilter::Nearest,
             ImageTextureWrap wrap = ImageTextureWrap::Repeat);

    int width(int level = 0) const;
    int height(int level = 0) const;
    int mipLevelCount() const;
    const std::vector<Colord>& pixels(int level = 0) const;

    ImageTextureFilter filter() const;
    ImageTextureWrap wrap() const;
    const TextureMapping2D* mapping() const;

    Colord sample(double u, double v) const;
    Colord sample(double u, double v, const Vector2d& duvdx, const Vector2d& duvdy) const;
    Colord sampleLevel(double u, double v, double level) const;
    double mipLevelForDerivatives(const Vector2d& duvdx, const Vector2d& duvdy) const;

    virtual Colord evaluate(const Rayd& ray, const HitPoint& hitPoint) const;

    static ImageTextureFilter filterFromString(const std::string& value);
    static ImageTextureWrap wrapFromString(const std::string& value);
    static const char* filterToString(ImageTextureFilter filter);
    static const char* wrapToString(ImageTextureWrap wrap);

  private:
    struct Level {
      int width;
      int height;
      std::vector<Colord> pixels;
    };

    Colord sampleNearest(int level, double u, double v) const;
    Colord sampleBilinear(int level, double u, double v) const;
    int texelIndex(int level, int x, int y) const;
    int wrapCoord(int coord, int size) const;
    double normalizedCoord(double value) const;
    void buildMipLevels();

    std::unique_ptr<TextureMapping2D> m_mapping;
    std::vector<Level> m_levels;
    ImageTextureFilter m_filter;
    ImageTextureWrap m_wrap;
  };
}
