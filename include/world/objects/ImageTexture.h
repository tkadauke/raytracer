#pragma once

#include "world/objects/Texture.h"

#include <QString>

/**
  * Editable wrapper for an image-backed color texture.
  *
  * Filtering is caller-selected: "nearest" preserves point sampling,
  * "bilinear" blends neighboring texels, and "mipmap" uses the runtime mip
  * chain when the renderer can provide UV gradients.
  *
  * @image html image_texture_filter_nearest.png "Image texture sampling"
  *
  * @see render::ImageTexture
  */
class ImageTexture : public Texture {
  Q_OBJECT
  Q_PROPERTY(QString path READ path WRITE setPath)
  Q_PROPERTY(QString filter READ filter WRITE setFilter)
  Q_PROPERTY(QString wrap READ wrap WRITE setWrap)
  Q_PROPERTY(QString mapping READ mapping WRITE setMapping)
  Q_PROPERTY(double uScale READ uScale WRITE setUScale)
  Q_PROPERTY(double vScale READ vScale WRITE setVScale)

public:
  explicit ImageTexture(Element* parent = nullptr);

  inline const QString& path() const {
    return m_path;
  }

  inline void setPath(const QString& path) {
    m_path = path;
  }

  inline const QString& filter() const {
    return m_filter;
  }

  inline void setFilter(const QString& filter) {
    m_filter = (filter == "bilinear" || filter == "mipmap") ? filter : "nearest";
  }

  inline const QString& wrap() const {
    return m_wrap;
  }

  inline void setWrap(const QString& wrap) {
    m_wrap = wrap == "clamp" ? "clamp" : "repeat";
  }

  inline const QString& mapping() const {
    return m_mapping;
  }

  inline void setMapping(const QString& mapping) {
    m_mapping = mapping == "planar" ? "planar" : "uv";
  }

  inline double uScale() const {
    return m_uScale;
  }

  inline void setUScale(double scale) {
    m_uScale = scale;
  }

  inline double vScale() const {
    return m_vScale;
  }

  inline void setVScale(double scale) {
    m_vScale = scale;
  }

  virtual std::shared_ptr<render::Texturec> toRaytracerTexture() const;

private:
  QString m_path;
  QString m_filter;
  QString m_wrap;
  QString m_mapping;
  double m_uScale;
  double m_vScale;
};
