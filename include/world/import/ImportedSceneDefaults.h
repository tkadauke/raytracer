#pragma once

#include "core/Color.h"
#include "core/math/BoundingBox.h"
#include "core/math/Vector.h"
#include "world/import/ImportOptions.h"

#include <QString>

#include <memory>

class Scene;
struct StepPlaybackStyle;

namespace world {

  /**
    * Parameters for world::addBoundsFramedCameraAndLight(): the numeric
    * knobs that differ between importers which each position a default
    * camera/light directly from an asset's bounding box, rather than via
    * ImportedSceneDefaults::frameCamera()'s runtime-scene auto-framing.
    */
  struct BoundsFramedViewSpec {
    Vector3d fallbackSize;
    double minDistanceFloor;
    double distanceMultiplier;
    Vector3d positionDirection;
    double zoom;
    QString cameraId;
    QString cameraName;
    QString lightId;
    QString lightName;
    Vector3d lightDirection;
  };

  /**
    * Adds a PinholeCamera and DirectionalLight to @p scene, positioned from
    * @p bounds: distance is `max(bounds.size(), spec.minDistanceFloor) *
    * spec.distanceMultiplier`, and the camera sits at
    * `bounds.center() + spec.positionDirection * distance`. Falls back to
    * @p spec.fallbackSize / the origin when @p bounds is invalid (empty
    * source geometry).
    *
    * Shared by importers (AdditiveManufacturingSceneImporter,
    * GCodeSceneImporter) whose default view is computed directly from the
    * imported geometry's bounds, as opposed to
    * ImportedSceneDefaults::frameCamera()'s runtime-scene auto-framing.
    */
  void addBoundsFramedCameraAndLight(Scene& scene, const BoundingBoxd& bounds,
                                     const BoundsFramedViewSpec& spec);

  /**
    * Shared product-view defaults for importers that turn a standalone asset
    * file into a complete editable scene.
    */
  class ImportedSceneDefaults {
  public:
    ImportedSceneDefaults();

    void setAmbientColor(const Colord& color);
    void setBackgroundColor(const Colord& color);
    void setAmbientColorFromOption(const ImportOptions& options, const QString& name,
                                   const QString& diagnosticName = QString());
    void setBackgroundColorFromOption(const ImportOptions& options, const QString& name,
                                      const QString& diagnosticName = QString());
    void setCameraDirection(const Vector3d& direction);
    void setLightDirection(const Vector3d& direction);

    [[nodiscard]] const Colord& ambientColor() const;
    [[nodiscard]] const Colord& backgroundColor() const;
    [[nodiscard]] const Vector3d& cameraDirection() const;
    [[nodiscard]] const Vector3d& lightDirection() const;

    [[nodiscard]] std::unique_ptr<Scene> createScene(const QString& name) const;
    void applyTo(Scene& scene) const;
    [[nodiscard]] bool frameCamera(Scene& scene) const;
    [[nodiscard]] bool frameCamera(Scene& scene, const StepPlaybackStyle& style) const;

  private:
    [[nodiscard]] Colord colorOption(const ImportOptions& options, const QString& name,
                                     const QString& diagnosticName, const Colord& fallback) const;
    [[nodiscard]] Colord parseColor(const QString& value, const QString& optionName) const;
    [[nodiscard]] bool sceneHasLight(const Scene& scene) const;

    Colord m_ambientColor;
    Colord m_backgroundColor;
    Vector3d m_cameraDirection;
    Vector3d m_lightDirection;
  };

}
