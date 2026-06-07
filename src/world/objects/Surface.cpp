#include "world/objects/Surface.h"
#include "world/objects/Material.h"
#include "world/objects/Group.h"
#include "world/objects/Light.h"
#include "world/objects/StepVisibilityEvaluator.h"
#include "engine/graph/RenderSceneAnalysis.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Scene.h"

#include <stdexcept>
#include <string>

Surface::Surface(Element* parent)
    : Transformable(parent),
      m_material(nullptr),
      m_renderTextureSubview(),
      m_visible(true),
      m_velocity(Vector3d::null),
      m_portalReceiverMarker(false),
      m_planarMirrorMarker(false) {
}

const QString& Surface::renderTextureSubview() const {
  return m_renderTextureSubview;
}

void Surface::setRenderTextureSubview(const QString& subviewName) {
  m_renderTextureSubview = subviewName.trimmed();
}

const QString& Surface::renderTextureSubview() const {
  return m_renderTextureSubview;
}

void Surface::setRenderTextureSubview(const QString& subviewName) {
  m_renderTextureSubview = subviewName.trimmed();
}

std::shared_ptr<render::Primitive>
Surface::applyTransform(std::shared_ptr<render::Primitive> primitive) const {
  auto result = std::make_shared<render::Instance>(primitive);
  result->setMatrix(localTransform());
  result->setVelocity(m_velocity);
  return result;
}

void Surface::read(const QJsonObject& json) {
  Element::read(json);
  validateSceneMarkers();
}

std::shared_ptr<render::Primitive> Surface::toRaytracer(render::Scene* scene) const {
  return toRaytracer(scene, StepPlaybackStyle());
}

std::shared_ptr<render::Primitive> Surface::toRaytracer(render::Scene* scene,
                                                        const StepPlaybackStyle& style) const {
  if (!visible())
    return nullptr;

  auto primitive = toRaytracerPrimitive();
  if (!primitive) {
    return primitive;
  }

  if (material()) {
    primitive->setMaterial(material()->toRaytracerMaterial());
  }

  if (childElements().size() > 0) {
    auto composite = std::dynamic_pointer_cast<render::Composite>(primitive);
    if (!composite) {
      composite = std::make_shared<render::Composite>();
      composite->add(primitive);
    }

    for (const auto& child : childElements()) {
      if (Surface* surface = qobject_cast<Surface*>(child)) {
        auto primitive = surface->toRaytracer(scene, style);
        if (primitive)
          composite->add(primitive);
      } else if (Group* group = qobject_cast<Group*>(child)) {
        auto primitive = group->toRaytracer(scene, style);
        if (primitive)
          composite->add(primitive);
      } else if (Light* light = qobject_cast<Light*>(child)) {
        if (light->visible())
          scene->addLight(light->toRaytracer());
      }
    }

    if (auto index = std::dynamic_pointer_cast<render::SpatialIndex>(composite)) {
      index->setup();
    }

    return applyTransform(composite);
  } else {
    return applyTransform(primitive);
  }
}

bool Surface::canHaveChild(Element* child) const {
  return dynamic_cast<Surface*>(child) != nullptr || dynamic_cast<Light*>(child) != nullptr ||
         dynamic_cast<Group*>(child) != nullptr;
}

void Surface::contributeToRenderGraphAnalysis(engine::graph::RenderSceneAnalysis& analysis) const {
  if (!visible()) {
    return;
  }
  validateSceneMarkers();
  analysis.recordVisibleSurface();
  if (portalReceiverMarker()) {
    analysis.recordPortalReceiverSurface(id().toStdString(), name().toStdString());
  }
  if (planarMirrorMarker()) {
    analysis.recordPlanarMirrorSurface(id().toStdString(), name().toStdString());
  }
  analysis.recordRenderTextureReceiver(m_renderTextureSubview.toStdString());
  Element::contributeToRenderGraphAnalysis(analysis);
}

bool Surface::isPropertyVisible(const QString& propertyName) const {
  if (propertyName == QStringLiteral("portalReceiverMarker") ||
      propertyName == QStringLiteral("planarMirrorMarker")) {
    return supportsPlanarSceneMarker();
  }
  return Transformable::isPropertyVisible(propertyName);
}

QString Surface::propertyDescription(const QString& propertyName) const {
  if (propertyName == QStringLiteral("portalReceiverMarker")) {
    return QStringLiteral(
      "Marks this planar surface as a portal receiver for automatic render graph discovery.");
  }
  if (propertyName == QStringLiteral("planarMirrorMarker")) {
    return QStringLiteral(
      "Marks this planar surface as a mirror surface for automatic render graph discovery.");
  }
  return Transformable::propertyDescription(propertyName);
}

QString Surface::propertyGroup(const QString& propertyName) const {
  if (propertyName == QStringLiteral("portalReceiverMarker") ||
      propertyName == QStringLiteral("planarMirrorMarker")) {
    return QStringLiteral("Scene Markers");
  }
  return Transformable::propertyGroup(propertyName);
}

bool Surface::supportsPlanarSceneMarker() const {
  return false;
}

std::string Surface::sceneMarkerDiagnosticPrefix() const {
  std::string prefix = "surface scene marker";
  if (!id().isEmpty()) {
    prefix += " on id '" + id().toStdString() + "'";
  }
  if (!name().isEmpty()) {
    prefix += " ('" + name().toStdString() + "')";
  }
  prefix += " [" + std::string(metaObject()->className()) + "]";
  return prefix;
}

void Surface::validateSceneMarkers() const {
  if (portalReceiverMarker() && planarMirrorMarker()) {
    throw std::invalid_argument(sceneMarkerDiagnosticPrefix() +
                                " cannot be both a portal receiver and a planar mirror");
  }
  if (portalReceiverMarker() && !supportsPlanarSceneMarker()) {
    throw std::invalid_argument(sceneMarkerDiagnosticPrefix() +
                                " portal receiver marker requires a planar surface");
  }
  if (planarMirrorMarker() && !supportsPlanarSceneMarker()) {
    throw std::invalid_argument(sceneMarkerDiagnosticPrefix() +
                                " planar mirror marker requires a planar surface");
  }
}
