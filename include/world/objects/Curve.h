#pragma once

#include "core/geometry/Polyline.h"
#include "render/primitives/Curve.h"
#include "world/objects/Surface.h"

#include <memory>

/**
  * Editable scene wrapper for imported polyline curves.
  *
  * Curves are used by importers for path-like geometry such as protein
  * backbones. A zero-width curve renders through curve overlay passes; a
  * finite width tessellates as either a ribbon or tube.
  */
class Curve : public Surface {
  Q_OBJECT
  Q_PROPERTY(double width READ width WRITE setWidth)
  Q_PROPERTY(QString tessellationMode READ tessellationMode WRITE setTessellationMode)

public:
  explicit Curve(Element* parent = nullptr);

  [[nodiscard]] const core::Polyline& polyline() const;
  void setPolyline(const core::Polyline& polyline);

  [[nodiscard]] double width() const;
  void setWidth(double width);

  [[nodiscard]] QString tessellationMode() const;
  void setTessellationMode(const QString& mode);

  void read(const QJsonObject& json) override;
  void write(QJsonObject& json) override;

protected:
  std::shared_ptr<render::Primitive> toRaytracerPrimitive() const override;

private:
  [[nodiscard]] render::Curve::TessellationMode renderMode() const;

  core::Polyline m_polyline;
  double m_width;
  QString m_tessellationMode;
};
