#include "world/objects/ElementFactory.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Grid.h"
#include "ScriptElementRegistry.h"
#include "world/objects/Material.h"
#include "world/objects/ScriptedSurface.h"

#include <QJSEngine>
#include <QFile>
#include <QFileInfo>
#include <QEvent>
#include <QTextStream>
#include <iostream>

ScriptedSurface::ScriptedSurface(Element* parent)
    : Surface(parent),
      m_engine(nullptr),
      m_blockDynamicPropertyEvent(false) {
}

void ScriptedSurface::setupEngine() {
  delete m_engine;
  m_engine = new QJSEngine;

  auto* registry = new ScriptElementRegistry(m_engine, m_engine);
  m_engine->globalObject().setProperty("__reg__", m_engine->newQObject(registry));
  m_this = m_engine->newQObject(this);

  // Inject JS constructor wrappers so scripts can keep using "new Box(parent)"
  // and "new Vector3(x, y, z)" unchanged.
  m_engine->evaluate("function Box(p)          { return __reg__.createBox(p); }\n"
                     "function Sphere(p)       { return __reg__.createSphere(p); }\n"
                     "function Cylinder(p)     { return __reg__.createCylinder(p); }\n"
                     "function Ring(p)         { return __reg__.createRing(p); }\n"
                     "function Union(p)        { return __reg__.createUnion(p); }\n"
                     "function Intersection(p) { return __reg__.createIntersection(p); }\n"
                     "function Difference(p)   { return __reg__.createDifference(p); }\n"
                     "function MinkowskiSum(p) { return __reg__.createMinkowskiSum(p); }\n"
                     "function ConvexHull(p)   { return __reg__.createConvexHull(p); }\n"
                     "function Vector3(x,y,z)  { return __reg__.createVector3(x,y,z); }\n");
}

void ScriptedSurface::setScriptName(const QString& name) {
  m_scriptName = name;
  clear();
  removeDynamicProperties();
  setupEngine();
  loadScript();

  QFileInfo fi(name);
  auto functionName = fi.baseName();

  // The constructor function is defined on the global object by evaluate() but
  // must be called with m_this as 'this' so that "this.create = ..." lands on
  // the ScriptedSurface QObject wrapper rather than the bare JS global.
  QJSValue ctor = m_engine->globalObject().property(functionName);
  if (ctor.isCallable()) {
    QJSValue result = ctor.callWithInstance(m_this);
    if (result.isError())
      handleError(result);
  }
}

void ScriptedSurface::removeDynamicProperties() {
  for (const auto& name : dynamicPropertyNames()) {
    setProperty(name, QVariant());
  }
}

void ScriptedSurface::loadScript() {
  QFile file(m_scriptName);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream in(&file);
  QString script = in.readAll();

  // evaluate() covers both syntax errors and runtime exceptions; check the
  // returned value rather than a separate checkSyntax step (removed in Qt6).
  QJSValue result = m_engine->evaluate(script, m_scriptName);
  if (result.isError()) {
    handleError(result);
    return;
  }

  QJSValue properties = m_engine->globalObject().property("properties");
  if (properties.isObject()) {
    // Iterate via QVariantMap since QJSValueIterator was removed in Qt6.
    QVariantMap propMap = properties.toVariant().toMap();
    for (auto it = propMap.begin(); it != propMap.end(); ++it) {
      QByteArray name = it.key().toLatin1();
      QString type = it.value().toString();
      QJSValue value = m_this.property(it.key());
      if (type == "double") {
        if (value.isNumber()) {
          setProperty(name.constData(), QVariant::fromValue(double(value.toNumber())));
        } else {
          setProperty(name.constData(), QVariant::fromValue(double(0.0)));
        }
      } else if (type == "int") {
        if (value.isNumber()) {
          setProperty(name.constData(), QVariant::fromValue(int(value.toInt())));
        } else {
          setProperty(name.constData(), QVariant::fromValue(int(0)));
        }
      } else if (type == "Material") {
        setProperty(name.constData(), QVariant::fromValue(static_cast<Material*>(nullptr)));
      }
    }
  }
}

void ScriptedSurface::clear() {
  for (const auto& element : childElements()) {
    if (element->isGenerated()) {
      removeChild(element);
      delete element;
    }
  }
}

QJSValue ScriptedSurface::jsCall(QJSValue obj, const QString& function, const QJSValueList& args) {
  QJSValue func = obj.property(function);
  QJSValue result = func.callWithInstance(m_this, args);
  if (result.isError())
    handleError(result);
  return result;
}

bool ScriptedSurface::functionDefined(QJSValue obj, const QString& function) const {
  return obj.property(function).isCallable();
}

void ScriptedSurface::handleError(const QJSValue& error) {
  std::cout << "Uncaught exception in script " << m_scriptName.toStdString() << ": "
            << error.toString().toStdString() << std::endl;

  QJSValue stack = error.property("stack");
  if (stack.isString()) {
    std::cout << stack.toString().toStdString() << std::endl;
  }
}

bool ScriptedSurface::event(QEvent* e) {
  if (!m_blockDynamicPropertyEvent && e->type() == QEvent::DynamicPropertyChange) {
    if (engineReady()) {
      auto pe = static_cast<QDynamicPropertyChangeEvent*>(e);
      auto prop = QString(pe->propertyName());

      // Push the new QObject value onto the JS-side wrapper so the
      // script's `this.X` reads see the update. Without this, scripts
      // that initialise their properties via `this.X = null` in the
      // constructor end up with a JS-shadow that masks the QObject
      // dynamic property — `setProperty("diceMaterial", red_matte)`
      // from C++ updates the QObject but the script's `this.diceMaterial`
      // still reads the original `null` from the shadow, and the
      // surface renders with a null material (i.e. black).
      QJSValue jsValue = m_engine->toScriptValue(property(prop.toLatin1().constData()));
      m_this.setProperty(prop, jsValue);

      auto funcName = prop;
      funcName[0] = funcName[0].toUpper();
      funcName = "set" + funcName;
      if (functionDefined(m_this, funcName)) {
        jsCall(m_this, funcName, QJSValueList() << jsValue);

        // setX may have validated/clamped the value. Sync the
        // resulting JS-side value back to the QObject; suppress the
        // event so we don't re-enter this branch.
        m_blockDynamicPropertyEvent = true;
        setProperty(prop.toLatin1().constData(), m_this.property(prop).toVariant());
        m_blockDynamicPropertyEvent = false;
      }
      clear();
      if (functionDefined(m_this, "create"))
        jsCall(m_this, "create");
    }
    return true;
  }
  return Surface::event(e);
}

std::shared_ptr<render::Primitive> ScriptedSurface::toRaytracerPrimitive() const {
  return make_named<render::Grid>();
}

static bool dummy = ElementFactory::self().registerClass<ScriptedSurface>("ScriptedSurface");
