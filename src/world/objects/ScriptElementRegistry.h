#pragma once

#include "core/math/Vector.h"
#include "world/objects/Box.h"
#include "world/objects/ConvexHull.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Difference.h"
#include "world/objects/Element.h"
#include "world/objects/Intersection.h"
#include "world/objects/Material.h"
#include "world/objects/MinkowskiSum.h"
#include "world/objects/Ring.h"
#include "world/objects/Sphere.h"
#include "world/objects/Union.h"

#include <QJSEngine>
#include <QJSValue>
#include <QObject>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Material*);

// Qt6 removed QScriptEngine::newFunction. Element creation is exposed to JS
// through Q_INVOKABLE methods on this helper. Constructor wrappers injected
// via evaluate() keep the existing script API ("new Box(parent)" etc.) intact:
// when a JS constructor returns an object, the 'new' expression forwards that
// object to the caller instead of the blank 'this' JS would otherwise allocate.
class ScriptElementRegistry : public QObject {
  Q_OBJECT
  QJSEngine* m_engine;

  template<class T>
  QJSValue createElement(QJSValue parentVal) {
    auto* parent = qobject_cast<Element*>(parentVal.toQObject());
    if (!parent)
      return QJSValue();
    auto* obj = new T(nullptr);
    obj->setGenerated(true);
    parent->addChild(obj);
    return m_engine->newQObject(obj);
  }

public:
  explicit ScriptElementRegistry(QJSEngine* engine, QObject* parent = nullptr)
    : QObject(parent), m_engine(engine) {}

  Q_INVOKABLE QJSValue createBox(QJSValue p)          { return createElement<Box>(p); }
  Q_INVOKABLE QJSValue createSphere(QJSValue p)       { return createElement<Sphere>(p); }
  Q_INVOKABLE QJSValue createCylinder(QJSValue p)     { return createElement<Cylinder>(p); }
  Q_INVOKABLE QJSValue createRing(QJSValue p)         { return createElement<Ring>(p); }
  Q_INVOKABLE QJSValue createUnion(QJSValue p)        { return createElement<Union>(p); }
  Q_INVOKABLE QJSValue createIntersection(QJSValue p) { return createElement<Intersection>(p); }
  Q_INVOKABLE QJSValue createDifference(QJSValue p)   { return createElement<Difference>(p); }
  Q_INVOKABLE QJSValue createMinkowskiSum(QJSValue p) { return createElement<MinkowskiSum>(p); }
  Q_INVOKABLE QJSValue createConvexHull(QJSValue p)   { return createElement<ConvexHull>(p); }

  Q_INVOKABLE QJSValue createVector3(double x, double y, double z) {
    return m_engine->toScriptValue(Vector3d(x, y, z));
  }
};
