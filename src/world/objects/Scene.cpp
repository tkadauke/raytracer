#include "world/objects/Scene.h"
#include "world/objects/Surface.h"
#include "world/objects/Camera.h"
#include "world/objects/Light.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Grid.h"

#include <QMap>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>

#include <stdexcept>
#include <utility>

Scene::Scene(Element* parent)
  : Element(parent),
    m_changed(false),
    m_ambient(Colord(0.4, 0.4, 0.4)),
    m_background(Colord(0.4, 0.8, 1))
{
  setName("New Scene");
}

std::shared_ptr<render::Scene> Scene::toRaytracerScene() const {
  auto result = std::make_shared<render::Scene>();

  auto grid = make_named<render::Grid>();
  for (const auto& child : childElements()) {
    if (auto surface = dynamic_cast<Surface*>(child)) {
      if (surface->visible()) {
        // Surface::toRaytracer takes a non-owning raw pointer — it only
        // reaches into the scene to register lights/elements.
        auto primitive = surface->toRaytracer(result.get());
        if (primitive && !primitive->boundingBox().isInfinite()) {
          grid->add(primitive);
        }
      }
    } else if (auto light = dynamic_cast<Light*>(child)) {
      if (light->visible()) {
        result->addLight(light->toRaytracer());
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

  Element::read(sceneJson);

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
  QFile file(filename);

  if (!file.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't read file.");
    return false;
  }

  QByteArray data = file.readAll();

  QJsonDocument loadDoc(QJsonDocument::fromJson(data));

  read(loadDoc.object());

  QMap<QString, Element*> references;
  findReferences(this, references);
  resolveReferences(references);

  return true;
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

void Scene::findReferences(Element* root, QMap<QString, Element*>& references) {
  references[root->id()] = root;

  for (const auto& child : root->childElements()) {
    findReferences(child, references);
  }
}

bool Scene::canHaveChild(Element*) const {
  return true;
}
