#pragma once
#include <algorithm>
#include <memory>

#include "world/objects/Material.h"
#include "core/Color.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"

/**
  * Redirects rays through a transformed view of the scene.
  *
  * `PortalMaterial` is evaluated at the surface hit point; it is not a
  * screen-space border or an image pasted onto the rectangle. When a ray
  * hits the portal surface, the material applies the inverse portal
  * transform to the shifted hit-point ray origin and to the ray direction,
  * asks the scene what that transformed ray sees, then multiplies the
  * returned color by the configured filter.
  *
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="portal_material_ray_redirection.js"></script>
  * @endhtmlonly
  *
  * <table><tr>
  * <td>@image html portal_material__raytracer.png "Raytracer"</td>
  * </tr></table>
  */
class PortalMaterial : public Material {
  Q_OBJECT
  Q_PROPERTY(Vector3d position READ position WRITE setPosition)
  Q_PROPERTY(Vector3d rotation READ rotation WRITE setRotation)
  Q_PROPERTY(Vector3d scale READ scale WRITE setScale)
  Q_PROPERTY(Colord filterColor READ filterColor WRITE setFilterColor)

public:
  /**
    * Constructs a portal at the origin with a white color filter.
    */
  explicit PortalMaterial(Element* parent = nullptr);

  /**
    * @returns the portal transform's translation.
    */
  inline const Vector3d& position() const {
    return m_position;
  }

  /**
    * Sets the portal transform's translation.
    */
  inline void setPosition(const Vector3d& position) {
    m_position = position;
  }

  /**
    * @returns the portal transform's Euler rotation in radians.
    */
  inline const Vector3d& rotation() const {
    return m_rotation;
  }

  /**
    * Sets the portal transform's Euler rotation in radians.
    */
  inline void setRotation(const Vector3d& rotation) {
    m_rotation = rotation;
  }

  /**
    * @returns the portal transform's scale.
    */
  inline const Vector3d& scale() const {
    return m_scale;
  }

  /**
    * Sets the portal transform's scale. Components are made positive and
    * clamped away from zero so the transform remains invertible.
    */
  inline void setScale(const Vector3d& scale) {
    m_scale =
      Vector3d(std::max(std::abs(scale.x()), 0.000001), std::max(std::abs(scale.y()), 0.000001),
               std::max(std::abs(scale.z()), 0.000001));
  }

  /**
    * @returns the color filter applied to redirected rays.
    */
  inline const Colord& filterColor() const {
    return m_filterColor;
  }

  /**
    * Sets the color filter applied to redirected rays.
    */
  inline void setFilterColor(const Colord& filterColor) {
    m_filterColor = filterColor;
  }

protected:
  virtual std::shared_ptr<render::Material> toRaytracerMaterial() const;

private:
  Matrix4d transformation() const;

  Vector3d m_position;
  Vector3d m_rotation;
  Vector3d m_scale;
  Colord m_filterColor;
};
