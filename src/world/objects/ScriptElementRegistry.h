#pragma once

#include <QJSValue>
#include <QObject>

class QJSEngine;

// Qt6 removed QScriptEngine::newFunction. Element creation is exposed to JS
// through Q_INVOKABLE methods on this helper. Constructor wrappers injected
// by ScriptedSurface keep the existing script API ("new Box(parent)" etc.).
class ScriptElementRegistry : public QObject {
  Q_OBJECT

public:
  explicit ScriptElementRegistry(QJSEngine* engine, QObject* parent = nullptr);

  Q_INVOKABLE QJSValue createBox(QJSValue parent);
  Q_INVOKABLE QJSValue createSphere(QJSValue parent);
  Q_INVOKABLE QJSValue createCylinder(QJSValue parent);
  Q_INVOKABLE QJSValue createRing(QJSValue parent);
  Q_INVOKABLE QJSValue createUnion(QJSValue parent);
  Q_INVOKABLE QJSValue createIntersection(QJSValue parent);
  Q_INVOKABLE QJSValue createDifference(QJSValue parent);
  Q_INVOKABLE QJSValue createMinkowskiSum(QJSValue parent);
  Q_INVOKABLE QJSValue createConvexHull(QJSValue parent);
  Q_INVOKABLE QJSValue createVector3(double x, double y, double z);

private:
  QJSEngine* m_engine;
};
