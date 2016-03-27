#pragma once

#include "world/objects/Surface.h"

#include <QScriptValue>

class QScriptEngine;

/**
  * This class provides a scripting interface for creating complex surfaces.
  * Scripts are written in Javascript.
  * 
  * The global object in a script is an instance of ScriptedSurface. This means
  * that scripts have access to this object's properties, such as the material.
  * 
  * The point of entrance for the script is a function create() with no
  * parameters. This function is called within the context of the current
  * object. Use this function to create the surface, by adding child objects to
  * the this object. The create function is called when the script is first
  * loaded, and whenever there is a change to the properties that might require
  * an update to the child objects.
  * 
  * @code
  * function create() {
  *   var box = new Box(this)
  *   box.position = new Vector3(2, 1, 1)
  * }
  * @endcode
  * 
  * You can expose properties by defining a variable for each property with a
  * suitable default, as well as a global _propTypes hash:
  * 
  * @code
  * var length = 10.0;
  * var mat = null;
  * var _propTypes = { 'length': 'double', 'mat': 'Material' }
  * @endcode
  * 
  * <table><tr>
  * <td>@image html dice.png "Dice rendered by using a ScriptedSurface"</td>
  * <td>@image html brick.png "Lego brick rendered by using a ScriptedSurface"</td>
  * </tr></table>
  */
class ScriptedSurface : public Surface {
  Q_OBJECT;
  Q_PROPERTY(QString scriptName READ scriptName WRITE setScriptName);
  
public:
  /**
    * Default constructor.
    */
  explicit ScriptedSurface(Element* parent = nullptr);

  /**
    * @returns the name of the script.
    */
  inline const QString& scriptName() const {
    return m_scriptName;
  }
  
  /**
    * Sets the name of the script. If a script was already set before, this will
    * reinitialize the scripting engine, remove all dynamic properties, and
    * clear all generated child elements.
    */
  void setScriptName(const QString& name);
  
protected:
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
  virtual bool event(QEvent *e);

  inline bool engineReady() const {
    return m_engine != nullptr;
  }

private:
  void removeDynamicProperties();
  void setupEngine();
  void loadScript();
  
  void clear();
  
  void handleError();
  
  QScriptValue jsCall(const QString& function);
  
  template<class T>
  void registerElement();
  
  QString m_scriptName;
  QScriptEngine* m_engine;
  QScriptValue m_this;
};
