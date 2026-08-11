#include "render/textures/ImageTexture.h"

#include "core/math/HitPoint.h"
#include "render/textures/TextureWrap.h"
#include "render/textures/mappings/TextureMapping2D.h"

#include <QColor>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace render;

ImageTexture::ImageTexture(TextureMapping2D* mapping, int width, int height,
                           const std::vector<Colord>& pixels, ImageTextureFilter filter,
                           ImageTextureWrap wrap)
    : m_mapping(mapping),
      m_filter(filter),
      m_wrap(wrap) {
  if (!mapping)
    throw std::invalid_argument("ImageTexture requires a texture mapping");
  if (width <= 0 || height <= 0)
    throw std::invalid_argument("ImageTexture dimensions must be positive");
  if (pixels.size() != static_cast<std::size_t>(width * height))
    throw std::invalid_argument("ImageTexture pixel count does not match dimensions");

  m_levels.push_back({width, height, pixels});
  buildMipLevels();
}

ImageTexture::~ImageTexture() = default;

const char* ImageTexture::typeName() const noexcept {
  return "ImageTexture";
}

std::shared_ptr<ImageTexture> ImageTexture::fromFile(TextureMapping2D* mapping,
                                                     const std::string& path,
                                                     ImageTextureFilter filter,
                                                     ImageTextureWrap wrap) {
  QImage image(QString::fromStdString(path));
  if (image.isNull())
    throw std::runtime_error("Unable to load image texture: " + path);

  const QImage converted = image.convertToFormat(QImage::Format_RGBA8888);
  std::vector<Colord> pixels;
  pixels.reserve(static_cast<std::size_t>(converted.width() * converted.height()));
  for (int y = 0; y != converted.height(); ++y) {
    for (int x = 0; x != converted.width(); ++x) {
      const QColor color = QColor::fromRgba(converted.pixel(x, y));
      pixels.emplace_back(color.redF(), color.greenF(), color.blueF());
    }
  }
  return std::make_shared<ImageTexture>(mapping, converted.width(), converted.height(), pixels,
                                        filter, wrap);
}

int ImageTexture::width(int level) const {
  return m_levels[std::clamp(level, 0, mipLevelCount() - 1)].width;
}

int ImageTexture::height(int level) const {
  return m_levels[std::clamp(level, 0, mipLevelCount() - 1)].height;
}

int ImageTexture::mipLevelCount() const {
  return static_cast<int>(m_levels.size());
}

const std::vector<Colord>& ImageTexture::pixels(int level) const {
  return m_levels[std::clamp(level, 0, mipLevelCount() - 1)].pixels;
}

ImageTextureFilter ImageTexture::filter() const {
  return m_filter;
}

ImageTextureWrap ImageTexture::wrap() const {
  return m_wrap;
}

const TextureMapping2D* ImageTexture::mapping() const {
  return m_mapping.get();
}

Colord ImageTexture::sample(double u, double v) const {
  if (m_filter == ImageTextureFilter::Nearest)
    return sampleNearest(0, u, v);
  return sampleBilinear(0, u, v);
}

Colord ImageTexture::sample(double u, double v, const Vector2d& duvdx,
                            const Vector2d& duvdy) const {
  if (m_filter == ImageTextureFilter::Mipmap)
    return sampleLevel(u, v, mipLevelForDerivatives(duvdx, duvdy));
  return sample(u, v);
}

Colord ImageTexture::sampleLevel(double u, double v, double level) const {
  if (m_filter == ImageTextureFilter::Nearest)
    return sampleNearest(0, u, v);
  if (m_filter == ImageTextureFilter::Bilinear)
    return sampleBilinear(0, u, v);

  const double clamped = std::clamp(level, 0.0, static_cast<double>(mipLevelCount() - 1));
  const int lower = static_cast<int>(std::floor(clamped));
  const int upper = std::min(lower + 1, mipLevelCount() - 1);
  const double t = clamped - lower;
  return sampleBilinear(lower, u, v).lerp(sampleBilinear(upper, u, v), t);
}

double ImageTexture::mipLevelForDerivatives(const Vector2d& duvdx, const Vector2d& duvdy) const {
  const double w = static_cast<double>(width());
  const double h = static_cast<double>(height());
  const double dx = std::sqrt(std::pow(duvdx.x() * w, 2.0) + std::pow(duvdx.y() * h, 2.0));
  const double dy = std::sqrt(std::pow(duvdy.x() * w, 2.0) + std::pow(duvdy.y() * h, 2.0));
  const double footprint = std::max({dx, dy, 1.0});
  return std::clamp(std::log2(footprint), 0.0, static_cast<double>(mipLevelCount() - 1));
}

Colord ImageTexture::evaluate(const Rayd&, const HitPoint& hitPoint) const {
  double u = 0.0;
  double v = 0.0;
  m_mapping->map(hitPoint, u, v);
  return sample(u, v);
}

ImageTextureFilter ImageTexture::filterFromString(const std::string& value) {
  if (value == "bilinear")
    return ImageTextureFilter::Bilinear;
  if (value == "mipmap")
    return ImageTextureFilter::Mipmap;
  return ImageTextureFilter::Nearest;
}

ImageTextureWrap ImageTexture::wrapFromString(const std::string& value) {
  if (value == "clamp")
    return ImageTextureWrap::Clamp;
  return ImageTextureWrap::Repeat;
}

const char* ImageTexture::filterToString(ImageTextureFilter filter) {
  switch (filter) {
  case ImageTextureFilter::Bilinear:
    return "bilinear";
  case ImageTextureFilter::Mipmap:
    return "mipmap";
  case ImageTextureFilter::Nearest:
    return "nearest";
  }
  return "nearest";
}

const char* ImageTexture::wrapToString(ImageTextureWrap wrap) {
  return wrap == ImageTextureWrap::Clamp ? "clamp" : "repeat";
}

Colord ImageTexture::sampleNearest(int level, double u, double v) const {
  const Level& image = m_levels[level];
  const int x =
    wrapCoord(static_cast<int>(std::floor(normalizedCoord(u) * image.width)), image.width);
  const int y =
    wrapCoord(static_cast<int>(std::floor(normalizedCoord(v) * image.height)), image.height);
  return image.pixels[texelIndex(level, x, y)];
}

Colord ImageTexture::sampleBilinear(int level, double u, double v) const {
  const Level& image = m_levels[level];
  const double x = normalizedCoord(u) * image.width - 0.5;
  const double y = normalizedCoord(v) * image.height - 0.5;
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const double tx = x - x0;
  const double ty = y - y0;

  const Colord c00 =
    image.pixels[texelIndex(level, wrapCoord(x0, image.width), wrapCoord(y0, image.height))];
  const Colord c10 =
    image.pixels[texelIndex(level, wrapCoord(x0 + 1, image.width), wrapCoord(y0, image.height))];
  const Colord c01 =
    image.pixels[texelIndex(level, wrapCoord(x0, image.width), wrapCoord(y0 + 1, image.height))];
  const Colord c11 =
    image
      .pixels[texelIndex(level, wrapCoord(x0 + 1, image.width), wrapCoord(y0 + 1, image.height))];

  return c00.lerp(c10, tx).lerp(c01.lerp(c11, tx), ty);
}

int ImageTexture::texelIndex(int level, int x, int y) const {
  return y * m_levels[level].width + x;
}

int ImageTexture::wrapCoord(int coord, int size) const {
  return wrapTexelCoordinate(coord, size, m_wrap == ImageTextureWrap::Clamp);
}

double ImageTexture::normalizedCoord(double value) const {
  return wrapUnitCoordinate(value, m_wrap == ImageTextureWrap::Clamp);
}

void ImageTexture::buildMipLevels() {
  while (m_levels.back().width > 1 || m_levels.back().height > 1) {
    const Level& previous = m_levels.back();
    const int nextWidth = std::max(1, (previous.width + 1) / 2);
    const int nextHeight = std::max(1, (previous.height + 1) / 2);
    std::vector<Colord> pixels;
    pixels.reserve(static_cast<std::size_t>(nextWidth * nextHeight));

    for (int y = 0; y != nextHeight; ++y) {
      for (int x = 0; x != nextWidth; ++x) {
        const int sx = x * 2;
        const int sy = y * 2;
        const Colord c00 = previous.pixels[std::min(sy, previous.height - 1) * previous.width +
                                           std::min(sx, previous.width - 1)];
        const Colord c10 = previous.pixels[std::min(sy, previous.height - 1) * previous.width +
                                           std::min(sx + 1, previous.width - 1)];
        const Colord c01 = previous.pixels[std::min(sy + 1, previous.height - 1) * previous.width +
                                           std::min(sx, previous.width - 1)];
        const Colord c11 = previous.pixels[std::min(sy + 1, previous.height - 1) * previous.width +
                                           std::min(sx + 1, previous.width - 1)];
        pixels.push_back((c00 + c10 + c01 + c11) / 4.0);
      }
    }
    m_levels.push_back({nextWidth, nextHeight, pixels});
  }
}
