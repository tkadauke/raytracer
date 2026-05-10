#include "world/objects/Element.h"
#include "world/objects/Box.h"
#include "world/objects/ConvexHull.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Difference.h"
#include "world/objects/Intersection.h"
#include "world/objects/Material.h"
#include "world/objects/MinkowskiSum.h"
#include "world/objects/Ring.h"
#include "world/objects/Sphere.h"
#include "world/objects/Union.h"
#include "core/math/Vector.h"
#include "ScriptElementRegistry.h"

#include <QJSEngine>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Material*);

namespace {
  template<class T>
  QJSValue createElement(QJSEngine* engine, QJSValue parentVal) {
    auto* parent = qobject_cast<Element*>(parentVal.toQObject());
    if (!parent)
      return QJSValue();
    auto* obj = new T(nullptr);
    obj->setGenerated(true);
    parent->addChild(obj);
    return engine->newQObject(obj);
  }
}

ScriptElementRegistry::ScriptElementRegistry(QJSEngine* engine, QObject* parent)
    : QObject(parent),
      m_engine(engine) {
}

QJSValue ScriptElementRegistry::createBox(QJSValue parent) {
  return createElement<Box>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createSphere(QJSValue parent) {
  return createElement<Sphere>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createCylinder(QJSValue parent) {
  return createElement<Cylinder>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createRing(QJSValue parent) {
  return createElement<Ring>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createUnion(QJSValue parent) {
  return createElement<Union>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createIntersection(QJSValue parent) {
  return createElement<Intersection>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createDifference(QJSValue parent) {
  return createElement<Difference>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createMinkowskiSum(QJSValue parent) {
  return createElement<MinkowskiSum>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createConvexHull(QJSValue parent) {
  return createElement<ConvexHull>(m_engine, parent);
}

QJSValue ScriptElementRegistry::createVector3(double x, double y, double z) {
  return m_engine->toScriptValue(Vector3d(x, y, z));
}
