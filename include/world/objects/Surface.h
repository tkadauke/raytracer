#pragma once
#include <memory>

#include "world/objects/Transformable.h"

namespace render {
  class Primitive;
  class Scene;
}

class Material;

/**
  * Abstract base class for visible objects. All Surfaces have a transformation
  * and material, and (optionally) a per-shutter linear `velocity` driving
  * motion blur.
  *
  * @image html motion_blur_hero.png "Motion blur on a red sphere with velocity (1.5, 0, 0) — stratified shutter sampling produces the smooth fade from leading to trailing edge."
  */
class Surface : public Transformable {
  Q_OBJECT
  Q_PROPERTY(bool visible READ visible WRITE setVisible)
  Q_PROPERTY(Material* material READ material WRITE setMaterial)
  Q_PROPERTY(Vector3d velocity READ velocity WRITE setVelocity)

public:
  /**
    * Default constructor.
    */
  explicit Surface(Element* parent = nullptr);
  
  /**
    * @returns true if the surface is visible, false otherwise.
    */
  inline bool visible() const {
    return m_visible;
  }
  
  /**
    * Sets the surface's visibility property to visible.
    */
  inline void setVisible(bool visible) {
    m_visible = visible;
  }
  
  /**
    * Sets the surface's visible flag to true.
    */
  inline void show() {
    setVisible(true);
  }
  
  /**
    * Sets the surface's visible flag to false.
    */
  inline void hide() {
    setVisible(false);
  }
  
  /**
    * @returns the surface's material.
    */
  inline Material* material() const {
    return m_material;
  }
  
  /**
    * Sets the surface's material.
    */
  inline void setMaterial(Material* material) {
    m_material = material;
  }

  /**
    * @returns the surface's per-shutter linear velocity. Zero
    * means the surface is static (no motion blur). Non-zero
    * means the surface translates from `position` to `position +
    * velocity` over the shutter window.
    */
  inline const Vector3d& velocity() const {
    return m_velocity;
  }

  /**
    * Sets the surface's per-shutter linear velocity. Drives
    * motion blur — the renderer's per-ray time sample
    * (`State::timeSample`, drawn from the sample stream's 1D
    * dimension) interpolates the position linearly between
    * `position` (at time 0) and `position + velocity` (at time
    * 1). At default zero, the surface is static and `intersect`
    * takes the fast no-motion path.
    *
    * Rotation and scale animation are not supported in this
    * first pass — only linear translation.
    *
    * The sweep below shows the X-component of `velocity` ramping
    * from 0 (static, crisp sphere) to 2 (heavily blurred, mostly
    * transparent). A stochastic sampler (Jittered, Random) is
    * required for the blur to render — `Regular` collapses every
    * primary ray's time sample to a constant and produces a sharp
    * render at the half-shutter position.
    *
    * For the runtime intersection diagram, see
    * `render::Instance::setVelocity`: the same `velocity` value is
    * converted into per-ray time-offset primitive intersections.
    *
    * <table><tr>
    * <td>@image html motion_blur_velocity_0.png "velocity=(0, 0, 0) — static"</td>
    * <td>@image html motion_blur_velocity_0.5.png "velocity=(0.5, 0, 0)"</td>
    * <td>@image html motion_blur_velocity_1.png "velocity=(1, 0, 0)"</td>
    * <td>@image html motion_blur_velocity_1.5.png "velocity=(1.5, 0, 0)"</td>
    * <td>@image html motion_blur_velocity_2.png "velocity=(2, 0, 0) — heavy blur"</td>
    * </tr></table>
    */
  inline void setVelocity(const Vector3d& velocity) {
    m_velocity = velocity;
  }

  /**
    * Converts this surface into a render::Primitive.
    */
  std::shared_ptr<render::Primitive> toRaytracer(render::Scene* scene) const;
  virtual bool canHaveChild(Element* child) const;
  
protected:
  virtual std::shared_ptr<render::Primitive> toRaytracerPrimitive() const = 0;
  std::shared_ptr<render::Primitive> applyTransform(std::shared_ptr<render::Primitive> primitive) const;
  
private:
  Material* m_material;

  bool m_visible;
  Vector3d m_velocity;
};
