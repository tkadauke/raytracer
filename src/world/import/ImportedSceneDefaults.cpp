#include "world/import/ImportedSceneDefaults.h"

#include "world/objects/DirectionalLight.h"
#include "world/objects/Light.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"

#include <QColor>
#include <QRegularExpression>

#include <stdexcept>

namespace world {

  ImportedSceneDefaults::ImportedSceneDefaults()
      : m_ambientColor(0.8, 0.8, 0.8),
        m_backgroundColor(Colord::white()),
        m_cameraDirection(0.0, 0.0, -1.0),
        m_lightDirection(-0.35, 0.7, -1.0) {
  }

  void ImportedSceneDefaults::setAmbientColor(const Colord& color) {
    m_ambientColor = color;
  }

  void ImportedSceneDefaults::setBackgroundColor(const Colord& color) {
    m_backgroundColor = color;
  }

  void ImportedSceneDefaults::setAmbientColorFromOption(const ImportOptions& options,
                                                        const QString& name,
                                                        const QString& diagnosticName) {
    m_ambientColor = colorOption(options, name, diagnosticName, m_ambientColor);
  }

  void ImportedSceneDefaults::setBackgroundColorFromOption(const ImportOptions& options,
                                                           const QString& name,
                                                           const QString& diagnosticName) {
    m_backgroundColor = colorOption(options, name, diagnosticName, m_backgroundColor);
  }

  void ImportedSceneDefaults::setCameraDirection(const Vector3d& direction) {
    m_cameraDirection = direction;
  }

  void ImportedSceneDefaults::setLightDirection(const Vector3d& direction) {
    m_lightDirection = direction;
  }

  const Colord& ImportedSceneDefaults::ambientColor() const {
    return m_ambientColor;
  }

  const Colord& ImportedSceneDefaults::backgroundColor() const {
    return m_backgroundColor;
  }

  const Vector3d& ImportedSceneDefaults::cameraDirection() const {
    return m_cameraDirection;
  }

  const Vector3d& ImportedSceneDefaults::lightDirection() const {
    return m_lightDirection;
  }

  std::unique_ptr<Scene> ImportedSceneDefaults::createScene(const QString& name) const {
    auto scene = std::make_unique<Scene>();
    scene->setName(name);
    applyTo(*scene);
    return scene;
  }

  void ImportedSceneDefaults::applyTo(Scene& scene) const {
    scene.setAmbient(m_ambientColor);
    scene.setBackground(m_backgroundColor);

    if (!scene.activeCamera()) {
      auto camera = std::make_unique<PinholeCamera>();
      camera->setId("camera");
      camera->setName("Camera");
      scene.addChild(std::move(camera));
    }

    if (!sceneHasLight(scene)) {
      auto light = std::make_unique<DirectionalLight>();
      light->setId("light");
      light->setName("Light");
      light->setDirection(m_lightDirection);
      scene.addChild(std::move(light));
    }
  }

  bool ImportedSceneDefaults::frameCamera(Scene& scene) const {
    return scene.frameActivePinholeCameraToContents(m_cameraDirection);
  }

  bool ImportedSceneDefaults::frameCamera(Scene& scene, const StepPlaybackStyle& style) const {
    return scene.frameActivePinholeCameraToContents(style, m_cameraDirection);
  }

  Colord ImportedSceneDefaults::colorOption(const ImportOptions& options, const QString& name,
                                            const QString& diagnosticName,
                                            const Colord& fallback) const {
    const QVariant value = options.value(name);
    if (!value.isValid())
      return fallback;
    return parseColor(value.toString(), diagnosticName.isEmpty() ? name : diagnosticName);
  }

  Colord ImportedSceneDefaults::parseColor(const QString& value, const QString& optionName) const {
    QString text = value.trimmed();
    if (text.isEmpty()) {
      throw std::invalid_argument(
        QString("%1 must be a color name or hex color").arg(optionName).toStdString());
    }

    if (text.startsWith("0x", Qt::CaseInsensitive))
      text = "#" + text.mid(2);

    static const QRegularExpression bareHex("^[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$");
    if (bareHex.match(text).hasMatch())
      text = "#" + text;

    const QColor color(text);
    if (!color.isValid()) {
      throw std::invalid_argument(
        QString("%1 must be a color name or hex color").arg(optionName).toStdString());
    }

    return Colord(color.redF(), color.greenF(), color.blueF());
  }

  bool ImportedSceneDefaults::sceneHasLight(const Scene& scene) const {
    for (Element* child : scene.childElements()) {
      if (qobject_cast<Light*>(child))
        return true;
    }
    return false;
  }

}
