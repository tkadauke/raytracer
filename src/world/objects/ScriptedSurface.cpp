#include "world/objects/ElementFactory.h"
#include "raytracer/primitives/Composite.h"
#include "world/objects/ScriptedSurface.h"
#include "world/objects/Box.h"
#include "world/objects/Sphere.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Union.h"
#include "world/objects/Intersection.h"
#include "world/objects/Difference.h"
#include "world/objects/Material.h"

#include <QScriptEngine>
#include <QFile>
#include <QEvent>
#include <QTextStream>
#include <QScriptValueIterator>
#include <QScriptSyntaxCheckResult>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Material*);

namespace {
  template<class T>
  QScriptValue ElementConstructor(QScriptContext* ctx, QScriptEngine* eng) {
    auto parent = ctx->argument(0).toQObject();
    if (Element* e = qobject_cast<Element*>(parent)) {
      T* b = new T(nullptr);
      b->setGenerated(true);
      e->addChild(b);
      return eng->newQObject(b);
    }
    return QScriptValue();
  }


  QScriptValue Vector3dConstructor(QScriptContext* ctx, QScriptEngine* eng) {
    double x = ctx->argument(0).toNumber();
    double y = ctx->argument(1).toNumber();
    double z = ctx->argument(2).toNumber();
    return eng->toScriptValue(Vector3d(x, y, z));
  }
}

ScriptedSurface::ScriptedSurface(Element* parent)
  : Surface(parent),
    m_engine(nullptr)
{
}

void ScriptedSurface::setupEngine() {
  m_engine = new QScriptEngine;
  m_this = m_engine->newQObject(this);
  m_engine->setGlobalObject(m_this);
  
  registerElement<Box>();
  registerElement<Sphere>();
  registerElement<Cylinder>();
  registerElement<Union>();
  registerElement<Intersection>();
  registerElement<Difference>();
  m_engine->globalObject().setProperty("Vector3", m_engine->newFunction(Vector3dConstructor));
}

void ScriptedSurface::setScriptName(const QString& name) {
  m_scriptName = name;
  clear();
  removeDynamicProperties();
  setupEngine();
  loadScript();
  
  jsCall("create");
}

void ScriptedSurface::removeDynamicProperties() {
  for (const auto& name : dynamicPropertyNames()) {
    setProperty(name, QVariant());
  }
}

template<class T>
void ScriptedSurface::registerElement() {
  QScriptValue ctor = m_engine->newFunction(ElementConstructor<T>);
  QScriptValue metaObject = m_engine->newQMetaObject(&T::staticMetaObject, ctor);
  m_engine->globalObject().setProperty(T::staticMetaObject.className(), metaObject);
}

void ScriptedSurface::loadScript() {
  QFile file(m_scriptName);
  
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream in(&file);
  QString script = in.readAll();
  
  auto result = QScriptEngine::checkSyntax(script);
  if (result.state() != QScriptSyntaxCheckResult::Valid) {
    std::cout << "Syntax error in script " << m_scriptName.toStdString()
              << " in line " << result.errorLineNumber()
              << " column " << result.errorColumnNumber()
              << ": " << result.errorMessage().toStdString() << std::endl;
    return;
  }
  
  m_engine->evaluate(script);
  if (m_engine->hasUncaughtException()) {
    handleError();
    return;
  }
  
  QScriptValue propTypes = m_engine->globalObject().property("_propTypes");
  if (propTypes.isObject()) {
    QScriptValueIterator it(propTypes);
    while (it.hasNext()) {
      it.next();
      
      const char* name = it.name().toStdString().c_str();
      QString type = it.value().toString();
      QScriptValue value = m_engine->globalObject().property(it.name());
      if (type == "double") {
        if (value.isNumber()) {
          setProperty(name, QVariant::fromValue(double(value.toNumber())));
        } else {
          setProperty(name, QVariant::fromValue(double(0.0)));
        }
      } else if (type == "Material") {
        setProperty(name, QVariant::fromValue(static_cast<Material*>(nullptr)));
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

QScriptValue ScriptedSurface::jsCall(const QString& function) {
  QScriptValue func = m_engine->globalObject().property(function);
  QScriptValue result = func.call(m_this);
  if (m_engine->hasUncaughtException()) {
    handleError();
  }
  return result;
}

void ScriptedSurface::handleError() {
  auto error = m_engine->uncaughtException();
  std::cout << "Uncaught exception in script " << m_scriptName.toStdString()
            << ": " << error.toString().toStdString() << std::endl;
  
  for (const auto& line : m_engine->uncaughtExceptionBacktrace()) {
    std::cout << line.toStdString() << std::endl;
  }
}

bool ScriptedSurface::event(QEvent *e) {
  if (e->type() == QEvent::DynamicPropertyChange) {
    if (engineReady()) {
      clear();
      jsCall("create");
    }
    return true;
  }
  return Surface::event(e);
}

std::shared_ptr<raytracer::Primitive> ScriptedSurface::toRaytracerPrimitive() const {
  return std::make_shared<raytracer::Composite>();
}

static bool dummy = ElementFactory::self().registerClass<ScriptedSurface>("ScriptedSurface");

#include "ScriptedSurface.moc"
