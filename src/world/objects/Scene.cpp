#include "world/objects/Scene.h"
#include "world/objects/Surface.h"
#include "world/objects/Camera.h"
#include "world/objects/Group.h"
#include "world/objects/Light.h"
#include "world/objects/PinholeCamera.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/RenderIntentElement.h"
#include "world/objects/StepVisibilityEvaluator.h"
#include "render/primitives/Scene.h"
#include "render/primitives/SpatialIndexFactory.h"
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
#include <vector>

namespace {
  bool looksLikeRenderGraphPlan(const QJsonObject& json) {
    return json["resources"].isArray() && json["passes"].isArray();
  }

  bool isKnownAccelerationMode(int mode) {
    return mode >= static_cast<int>(render::AccelerationMode::Automatic) &&
           mode <= static_cast<int>(render::AccelerationMode::BVH);
  }

  std::size_t countFiniteLeaves(const std::vector<std::shared_ptr<render::Primitive>>& primitives) {
    std::size_t count = 0;
    for (const auto& primitive : primitives) {
      primitive->forEachTransformedLeaf([&](const render::Primitive::TransformedLeaf& leaf) {
        const auto bounds = leaf.boundingBox();
        if (bounds.isValid() && !bounds.isInfinite()) {
          ++count;
        }
      });
    }
    return count;
  }
}

Scene::Scene(Element* parent)
    : Element(parent),
      m_changed(false),
      m_ambient(Colord(0.4, 0.4, 0.4)),
      m_background(Colord(0.4, 0.8, 1)),
      m_environmentRadiance(Colord::black()) {
  setName("New Scene");
  addChild(std::make_unique<RenderIntentElement>());
}

std::shared_ptr<render::Scene> Scene::toRaytracerScene() const {
  return toRaytracerScene(StepPlaybackStyle());
}

std::shared_ptr<render::Scene> Scene::toRaytracerScene(const StepPlaybackStyle& style) const {
  auto result = std::make_shared<render::Scene>();

  std::vector<std::shared_ptr<render::Primitive>> boundedPrimitives;
  for (const auto& child : childElements()) {
    if (auto surface = dynamic_cast<Surface*>(child)) {
      // Surface::toRaytracer takes a non-owning raw pointer — it only
      // reaches into the scene to register lights/elements.
      auto primitive = surface->toRaytracer(result.get(), style);
      if (primitive && !primitive->boundingBox().isInfinite()) {
        boundedPrimitives.push_back(primitive);
      }
    } else if (auto light = dynamic_cast<Light*>(child)) {
      if (light->visible()) {
        result->addLight(light->toRaytracer());
      }
    } else if (auto group = dynamic_cast<Group*>(child)) {
      auto primitive = group->toRaytracer(result.get(), style);
      if (primitive && !primitive->boundingBox().isInfinite()) {
        boundedPrimitives.push_back(primitive);
      }
    }
  }

  const render::AccelerationAnalysis analysis{boundedPrimitives.size(),
                                              countFiniteLeaves(boundedPrimitives)};
  const auto decision = accelerationPolicy().choose(analysis);
  result->setAccelerationDecision(decision);

  if (!boundedPrimitives.empty()) {
    auto geometry = render::makeSpatialIndex(decision.spatialIndexKind);
    render::spatialIndexPrimitive(geometry)->setName(name().toStdString());
    for (const auto& primitive : boundedPrimitives) {
      geometry->add(primitive);
    }
    geometry->setup();
    result->add(render::spatialIndexPrimitive(geometry));
  }

  result->setAmbient(ambient());
  result->setBackground(background());
  result->setEnvironmentRadiance(environmentRadiance());

  return result;
}

int Scene::accelerationMode() const {
  return static_cast<int>(m_accelerationMode);
}

void Scene::setAccelerationMode(int mode) {
  if (isKnownAccelerationMode(mode)) {
    m_accelerationMode = static_cast<render::AccelerationMode>(mode);
  } else {
    m_accelerationMode = render::AccelerationMode::Automatic;
  }
}

render::AccelerationPolicy Scene::accelerationPolicy() const {
  return render::AccelerationPolicy(m_accelerationMode);
}

QList<int> Scene::propertyIntChoices(const QString& propertyName) const {
  if (propertyName == QStringLiteral("accelerationMode")) {
    return {static_cast<int>(render::AccelerationMode::Automatic),
            static_cast<int>(render::AccelerationMode::Linear),
            static_cast<int>(render::AccelerationMode::Grid),
            static_cast<int>(render::AccelerationMode::BVH)};
  }
  return Element::propertyIntChoices(propertyName);
}

QString Scene::propertyChoiceDisplayName(const QString& propertyName, const QString& choice) const {
  if (propertyName == QStringLiteral("accelerationMode")) {
    const auto mode = static_cast<render::AccelerationMode>(choice.toInt());
    if (mode == render::AccelerationMode::Automatic)
      return QStringLiteral("Auto");
    if (mode == render::AccelerationMode::Linear)
      return QStringLiteral("Linear");
    if (mode == render::AccelerationMode::Grid)
      return QStringLiteral("Grid");
    if (mode == render::AccelerationMode::BVH)
      return QStringLiteral("BVH");
  }
  return Element::propertyChoiceDisplayName(propertyName, choice);
}

void Scene::read(const QJsonObject& json) {
  const auto type = json["type"];
  if (type.isString() && type.toString() != QStringLiteral("Scene")) {
    throw std::invalid_argument("scene JSON root type must be Scene");
  }
  if (type.isUndefined() && looksLikeRenderGraphPlan(json)) {
    throw std::invalid_argument(
      "render graph plan JSON cannot be opened as a scene; open a scene JSON in Modeler or use "
      "rendercli --render_graph_in to replay the graph plan");
  }

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
    m_evaluatedAnimationFrame.reset();
    return;
  }

  if (!animationValue.isObject())
    throw std::invalid_argument("scene animation must be an object");

  m_animation = std::make_unique<world::Timeline>(world::Timeline::read(animationValue.toObject()));
  m_evaluatedAnimationFrame.reset();
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

  QJsonParseError error;
  QJsonDocument loadDoc(QJsonDocument::fromJson(data, &error));
  if (error.error != QJsonParseError::NoError || !loadDoc.isObject()) {
    qWarning("Couldn't parse scene file.");
    return false;
  }

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
  m_evaluatedAnimationFrame.reset();
}

bool Scene::hasAnimation() const {
  return static_cast<bool>(m_animation);
}

std::optional<int> Scene::evaluatedAnimationFrame() const {
  return m_evaluatedAnimationFrame;
}

std::vector<world::AnimationTrackClassification> Scene::animationTrackClassifications() const {
  if (!m_animation)
    return {};

  return m_animation->classifyTracks(*this);
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

engine::graph::RenderSceneAnalysis Scene::renderGraphAnalysis() const {
  engine::graph::RenderSceneAnalysis analysis;
  contributeToRenderGraphAnalysis(analysis);
  return analysis;
}

void Scene::evaluateAnimationAtFrame(int frame) {
  if (m_animation) {
    m_animation->apply(*this, frame);
    m_evaluatedAnimationFrame = frame;
  }
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

std::vector<const Camera*> Scene::cameras() const {
  std::vector<const Camera*> result;
  collectCameras(this, result);
  return result;
}

void Scene::collectCameras(const Element* root, std::vector<const Camera*>& cameras) const {
  if (const auto* camera = qobject_cast<const Camera*>(root)) {
    cameras.push_back(camera);
  }
  for (const auto* child : root->childElements()) {
    collectCameras(child, cameras);
  }
}

const Camera* Scene::cameraById(const QString& id) const {
  if (id.isEmpty()) {
    return nullptr;
  }
  return qobject_cast<const Camera*>(findById(id));
}

const Camera*
Scene::cameraForRenderCameraRef(const engine::graph::RenderCameraRef& cameraRef) const {
  if (!cameraRef.sceneCameraId) {
    return nullptr;
  }
  return cameraById(QString::fromStdString(*cameraRef.sceneCameraId));
}

std::shared_ptr<render::Camera>
Scene::toRaytracerCameraForRenderCameraRef(const engine::graph::RenderCameraRef& cameraRef) const {
  const Camera* camera = cameraForRenderCameraRef(cameraRef);
  return camera ? camera->toRaytracer() : nullptr;
}

const Camera* Scene::cameraForRenderIntent(const engine::graph::RenderIntent& intent) const {
  const engine::graph::RenderIntent frameIntent = intent.withWholeFrameOverridesApplied();
  if (frameIntent.defaultCamera) {
    return cameraForRenderCameraRef(*frameIntent.defaultCamera);
  }
  return activeCamera();
}

std::shared_ptr<render::Camera>
Scene::toRaytracerCameraForRenderIntent(const engine::graph::RenderIntent& intent) const {
  const Camera* camera = cameraForRenderIntent(intent);
  return camera ? camera->toRaytracer() : nullptr;
}

bool Scene::frameActivePinholeCameraToContents(const Vector3d& targetToEyeDirection) {
  return frameActivePinholeCameraToContents(StepPlaybackStyle(), targetToEyeDirection);
}

bool Scene::frameActivePinholeCameraToContents(const StepPlaybackStyle& style,
                                               const Vector3d& targetToEyeDirection) {
  auto* camera = qobject_cast<PinholeCamera*>(activeCamera());
  if (!camera)
    return false;

  const auto runtimeScene = toRaytracerScene(style);
  return camera->frameFrom(runtimeScene->boundingBox(), targetToEyeDirection);
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
