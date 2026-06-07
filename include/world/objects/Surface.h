#pragma once
#include <memory>
#include <string>

#include <QString>

#include "world/objects/Transformable.h"

namespace render {
  class Primitive;
  class Scene;
}

class Material;
struct StepPlaybackStyle;

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
  Q_PROPERTY(QString renderTextureSubview READ renderTextureSubview WRITE setRenderTextureSubview)
  Q_PROPERTY(Vector3d velocity READ velocity WRITE setVelocity)
  Q_PROPERTY(bool portalReceiverMarker READ portalReceiverMarker WRITE setPortalReceiverMarker)
  Q_PROPERTY(bool planarMirrorMarker READ planarMirrorMarker WRITE setPlanarMirrorMarker)

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

  const QString& renderTextureSubview() const;
  void setRenderTextureSubview(const QString& subviewName);

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

  inline bool portalReceiverMarker() const {
    return m_portalReceiverMarker;
  }

  inline void setPortalReceiverMarker(bool enabled) {
    m_portalReceiverMarker = enabled;
  }

  inline bool planarMirrorMarker() const {
    return m_planarMirrorMarker;
  }

  inline void setPlanarMirrorMarker(bool enabled) {
    m_planarMirrorMarker = enabled;
  }

  /**
    * Converts this surface into a render::Primitive.
    */
  void read(const QJsonObject& json) override;
  std::shared_ptr<render::Primitive> toRaytracer(render::Scene* scene) const;
  std::shared_ptr<render::Primitive> toRaytracer(render::Scene* scene,
                                                 const StepPlaybackStyle& style) const;
  void contributeToRenderGraphAnalysis(engine::graph::RenderSceneAnalysis& analysis) const override;
  bool canHaveChild(Element* child) const override;
  bool isPropertyVisible(const QString& propertyName) const override;
  QString propertyDescription(const QString& propertyName) const override;
  QString propertyGroup(const QString& propertyName) const override;

protected:
  virtual std::shared_ptr<render::Primitive> toRaytracerPrimitive() const = 0;
  std::shared_ptr<render::Primitive>
  applyTransform(std::shared_ptr<render::Primitive> primitive) const;
  virtual bool supportsPlanarSceneMarker() const;

private:
  void validateSceneMarkers() const;
  std::string sceneMarkerDiagnosticPrefix() const;

  Material* m_material;
  QString m_renderTextureSubview;

  bool m_visible;
  Vector3d m_velocity;
  bool m_portalReceiverMarker;
  bool m_planarMirrorMarker;
};
