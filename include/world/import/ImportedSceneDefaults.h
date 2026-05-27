#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "world/import/ImportOptions.h"

#include <QString>

#include <memory>

class Scene;
struct StepPlaybackStyle;

namespace world {

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
