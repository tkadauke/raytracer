#pragma once

#include <memory>
#include <vector>

#include <QString>
#include <QVariant>
#include <QVariantList>

#include "world/objects/Surface.h"

/**
  * Represents a finite-width polyline curve.
  *
  * Curves store generic ordered 3D points and display options, then convert to
  * the runtime curve primitive for mesh-consuming render paths.
  */
class Curve : public Surface {
  Q_OBJECT
  Q_PROPERTY(QVariantList points READ points WRITE setPoints)
  Q_PROPERTY(double width READ width WRITE setWidth)
  Q_PROPERTY(QString tessellationMode READ tessellationMode WRITE setTessellationMode)

public:
  /**
    * Default constructor. Creates an empty ribbon curve with width 0.1.
    */
  explicit Curve(Element* parent = nullptr);

  /**
    * @returns the curve points as JSON-friendly `[x, y, z]` arrays.
    */
  QVariantList points() const;

  /**
    * Sets the curve points from JSON-friendly `[x, y, z]` arrays. Malformed
    * entries are ignored so imported scenes keep any valid points they contain.
    */
  void setPoints(const QVariantList& points);

  /**
    * @returns the curve points as typed vectors.
    */
  inline const std::vector<Vector3d>& pointVectors() const {
    return m_points;
  }

  /**
    * Replaces the curve points with typed vectors.
    */
  inline void setPointVectors(const std::vector<Vector3d>& points) {
    m_points = points;
  }

  /**
    * Adds a point to the end of the curve.
    */
  inline void addPoint(const Vector3d& point) {
    m_points.push_back(point);
  }

  /**
    * @returns the finite display width used for curve tessellation.
    */
  inline double width() const {
    return m_width;
  }

  /**
    * Sets the finite display width. Negative values are converted to positive
    * values; zero is allowed and produces no tessellated faces.
    */
  void setWidth(double width);

  /**
    * @returns `"ribbon"` or `"tube"`.
    */
  inline const QString& tessellationMode() const {
    return m_tessellationMode;
  }

  /**
    * Sets the tessellation mode. Unknown values fall back to `"ribbon"`.
    */
  void setTessellationMode(const QString& mode);

  virtual std::shared_ptr<render::Primitive> toRaytracerPrimitive() const;

private:
  std::vector<Vector3d> m_points;
  double m_width;
  QString m_tessellationMode;
};
