#include "world/objects/Scene.h"
#include "world/objects/Surface.h"
#include "world/objects/Camera.h"
#include "world/objects/Group.h"
#include "world/objects/Light.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/StepVisibilityEvaluator.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Grid.h"
#include "world/import/LDrawSceneImporter.h"

#include <QDir>
#include <QMap>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QVariant>

#include <stdexcept>
#include <utility>

Scene::Scene(Element* parent)
    : Element(parent),
      m_changed(false),
      m_ambient(Colord(0.4, 0.4, 0.4)),
      m_background(Colord(0.4, 0.8, 1)) {
  setName("New Scene");
}

std::shared_ptr<render::Scene> Scene::toRaytracerScene() const {
  return toRaytracerScene(StepPlaybackStyle());
}

std::shared_ptr<render::Scene> Scene::toRaytracerScene(const StepPlaybackStyle& style) const {
  auto result = std::make_shared<render::Scene>();

  auto grid = make_named<render::Grid>();
  for (const auto& child : childElements()) {
    if (auto surface = dynamic_cast<Surface*>(child)) {
      // Surface::toRaytracer takes a non-owning raw pointer — it only
      // reaches into the scene to register lights/elements.
      auto primitive = surface->toRaytracer(result.get(), style);
      if (primitive && !primitive->boundingBox().isInfinite()) {
        grid->add(primitive);
      }
    } else if (auto light = dynamic_cast<Light*>(child)) {
      if (light->visible()) {
        result->addLight(light->toRaytracer());
      }
    } else if (auto group = dynamic_cast<Group*>(child)) {
      auto primitive = group->toRaytracer(result.get(), style);
      if (primitive && !primitive->boundingBox().isInfinite()) {
        grid->add(primitive);
      }
    }
  }

  if (grid->primitives().size() > 0) {
    grid->setup();
    result->add(grid);
  }

  result->setAmbient(ambient());
  result->setBackground(background());

  return result;
}

void Scene::read(const QJsonObject& json) {
  auto sceneJson = json;
  sceneJson.remove("animation");
  sceneJson.remove("imports");
  sceneJson.remove("renderIntent");

  Element::read(sceneJson);
  readImports(json);

  const auto renderIntentValue = json["renderIntent"];
  if (renderIntentValue.isUndefined()) {
    m_renderIntent = engine::graph::RenderIntent();
    m_hasRenderIntent = false;
  } else {
    if (!renderIntentValue.isObject())
      throw std::invalid_argument("scene renderIntent must be an object");

    m_renderIntent = engine::graph::RenderIntent::fromJson(renderIntentValue.toObject());
    m_hasRenderIntent = true;
  }

  const auto animationValue = json["animation"];
  if (animationValue.isUndefined()) {
    m_animation.reset();
    return;
  }

  if (!animationValue.isObject())
    throw std::invalid_argument("scene animation must be an object");

  m_animation = std::make_unique<world::Timeline>(world::Timeline::read(animationValue.toObject()));
}

void Scene::write(QJsonObject& json) {
  Element::write(json);

  if (m_hasRenderIntent) {
    json["renderIntent"] = m_renderIntent.toJson();
  }

  if (m_animation) {
    QJsonObject animationObject;
    m_animation->write(animationObject);
    json["animation"] = animationObject;
  }
}

bool Scene::save(const QString& filename) {
  QFile file(filename);

  if (!file.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't write file.");
    return false;
  }

  QJsonObject object;
  write(object);

  QJsonDocument saveDoc(object);
  file.write(saveDoc.toJson());

  m_changed = false;

  return true;
}

bool Scene::load(const QString& filename) {
  return load(filename, QString());
}

bool Scene::load(const QString& filename, const QString& ldrawLibraryRootOverride) {
  clearImportDiagnostics();
  QFile file(filename);

  if (!file.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't read file.");
    return false;
  }

  QByteArray data = file.readAll();

  QJsonDocument loadDoc(QJsonDocument::fromJson(data));

  try {
    setProperty("_sourceFile", filename);
    read(loadDoc.object());
    setProperty("_sourceFile", QVariant());
  } catch (...) {
    setProperty("_sourceFile", QVariant());
    throw;
  }

  resolveElementReferences();
  world::imports::resolveLDrawAuthoringImports(
    this, ldrawLibraryRootOverride, QFileInfo(filename).absolutePath(), &m_importDiagnostics);

  return true;
}

void Scene::resolveElementReferences() {
  QMap<QString, Element*> references;
  findReferences(this, references);
  resolveReferences(references);
}

const std::vector<LDrawDiagnostic>& Scene::importDiagnostics() const {
  return m_importDiagnostics;
}

void Scene::setImportDiagnostics(std::vector<LDrawDiagnostic> diagnostics) {
  m_importDiagnostics = std::move(diagnostics);
}

void Scene::clearImportDiagnostics() {
  m_importDiagnostics.clear();
}

const world::Timeline* Scene::animation() const {
  return m_animation.get();
}

void Scene::setAnimation(std::unique_ptr<world::Timeline> animation) {
  m_animation = std::move(animation);
}

bool Scene::hasAnimation() const {
  return static_cast<bool>(m_animation);
}

const engine::graph::RenderIntent& Scene::renderIntent() const {
  return m_renderIntent;
}

void Scene::setRenderIntent(engine::graph::RenderIntent intent) {
  m_renderIntent = std::move(intent);
  m_hasRenderIntent = true;
}

void Scene::clearRenderIntent() {
  m_renderIntent = engine::graph::RenderIntent();
  m_hasRenderIntent = false;
}

bool Scene::hasRenderIntent() const {
  return m_hasRenderIntent;
}

engine::graph::RenderIntent Scene::renderIntentWithActiveCameraDefault() const {
  engine::graph::RenderIntent intent =
    hasRenderIntent() ? renderIntent() : engine::graph::RenderIntent();
  if (!intent.defaultCamera) {
    intent.defaultCamera = activeRenderCameraRef();
  }
  return intent;
}

void Scene::evaluateAnimationAtFrame(int frame) {
  if (m_animation)
    m_animation->apply(*this, frame);
}

std::unique_ptr<Scene> Scene::evaluatedAtFrame(int frame) const {
  QJsonObject json;
  const_cast<Scene*>(this)->write(json);

  auto result = std::make_unique<Scene>();
  result->read(json);

  QMap<QString, Element*> references;
  result->findReferences(result.get(), references);
  result->resolveReferences(references);

  result->evaluateAnimationAtFrame(frame);
  return result;
}

Camera* Scene::activeCamera() const {
  Camera* camera = nullptr;
  for (const auto& child : childElements()) {
    if (qobject_cast<Camera*>(child)) {
      camera = static_cast<Camera*>(child);
    }
  }
  return camera;
}

std::optional<engine::graph::RenderCameraRef> Scene::activeRenderCameraRef() const {
  const Camera* camera = activeCamera();
  if (!camera || camera->id().isEmpty())
    return std::nullopt;

  return engine::graph::RenderCameraRef{camera->id().toStdString(), std::nullopt};
}

void Scene::findReferences(Element* root, QMap<QString, Element*>& references) {
  references[root->id()] = root;

  for (const auto& child : root->childElements()) {
    findReferences(child, references);
  }
}

void Scene::readImports(const QJsonObject& json) {
  const auto importsValue = json["imports"];
  if (importsValue.isUndefined()) {
    return;
  }
  if (!importsValue.isArray())
    throw std::invalid_argument("scene imports must be an array");

  const QString basePath = QFileInfo(property("_sourceFile").toString()).absolutePath();
  const QDir baseDir(basePath.isEmpty() ? QDir::currentPath() : basePath);
  for (const auto& importValue : importsValue.toArray()) {
    if (!importValue.isObject())
      throw std::invalid_argument("scene import entry must be an object");

    const QJsonObject importObject = importValue.toObject();
    const QString source = importObject["source"].toString().trimmed();
    if (source.isEmpty())
      throw std::invalid_argument("scene import source must not be empty");

    const QString resolvedSource =
      QFileInfo(source).isRelative() ? baseDir.filePath(source) : source;
    std::unique_ptr<world::SceneImporter> importer;
    const QString format = importObject["format"].toString().trimmed();
    if (format.isEmpty()) {
      importer = world::SceneImporterRegistry::self().createForFile(resolvedSource);
    } else {
      importer = world::SceneImporterRegistry::self().createByFormat(format);
    }
    if (!importer) {
      throw std::invalid_argument(
        QString("No scene importer registered for %1")
          .arg(format.isEmpty() ? QFileInfo(resolvedSource).suffix() : format)
          .toStdString());
    }

    const auto optionsValue = importObject["options"];
    if (!optionsValue.isUndefined() && !optionsValue.isObject())
      throw std::invalid_argument("scene import options must be an object");

    world::ImportOptions options(optionsValue.toObject());
    world::ImportResult result = importer->importFile(resolvedSource, options);
    for (const auto& diagnostic : result.diagnostics()) {
      const char* severity = diagnostic.isError() ? "error" : "warning";
      qWarning("Import %s: %s", severity, qPrintable(diagnostic.message));
    }
    if (result.failed()) {
      throw std::invalid_argument(
        QString("Unable to import scene source: %1").arg(resolvedSource).toStdString());
    }

    std::unique_ptr<Element> root = result.takeRoot();
    if (auto* importedScene = qobject_cast<Scene*>(root.get())) {
      while (!importedScene->childElements().empty()) {
        addChild(importedScene->childElements().front());
      }
    } else {
      addChild(std::move(root));
    }
  }
}

bool Scene::canHaveChild(Element*) const {
  return true;
}
