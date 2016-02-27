#include "world/objects/Scene.h"
#include "world/objects/Surface.h"
#include "world/objects/Camera.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/primitives/Grid.h"

#include <QMap>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>

Scene::Scene(Element* parent)
  : Element(parent), m_changed(false)
{
}

raytracer::Scene* Scene::toRaytracerScene() const {
  raytracer::Scene* result = new raytracer::Scene;
  auto grid = std::make_shared<raytracer::Grid>();
  
  for (const auto& child : children()) {
    auto surface = dynamic_cast<Surface*>(child);
    if (surface && surface->visible()) {
      auto primitive = surface->toRaytracer();
      grid->add(primitive);
    }
  }
  
  if (grid->primitives().size()) {
    grid->setup();
    result->add(grid);
  }
  
  result->setAmbient(Colord(0.4, 0.4, 0.4));
  
  return result;
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

Camera* Scene::activeCamera() const {
  Camera* camera = nullptr;
  for (const auto& child : children()) {
    if (qobject_cast<Camera*>(child)) {
      camera = static_cast<Camera*>(child);
    }
  }
  return camera;
}

void Scene::findReferences(Element* root, QMap<QString, Element*>& references) {
  references[root->id()] = root;
  
  for (const auto& child : root->children()) {
    Element* e = qobject_cast<Element*>(child);
    if (e) {
      findReferences(e, references);
    }
  }
}

#include "Scene.moc"
