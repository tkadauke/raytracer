#pragma once

#include "world/objects/Element.h"
#include "core/Color.h"

class Camera;

namespace raytracer {
  class Scene;
}

class Scene : public Element {
  Q_OBJECT;
  Q_PROPERTY(Colord ambient READ ambient WRITE setAmbient);
  Q_PROPERTY(Colord background READ background WRITE setBackground);
  
public:
  /**
    * Default constructor. Constructs an empty scene with a white ambient
    * color and a white background color.
    */
  explicit Scene(Element* parent = nullptr);
  
  /**
    * Converts the scene into a representation suitable for rendering with the
    * Raytracer class.
    */
  raytracer::Scene* toRaytracerScene() const;
  
  /**
    * Saves the scene into a file specified by filename.
    */
  bool save(const QString& filename);
  
  /**
    * Loads the scene from the file specified by filename. This method will
    * treat the top level object in the file as this scene and recursively
    * create children for the child objects in the file.
    */
  bool load(const QString& filename);
  
  /**
    * @returns true if the scene was changed, false otherwise.
    */
  inline bool changed() const {
    return m_changed;
  }
  
  /**
    * Sets the changed flag of the scene.
    */
  inline void setChanged(bool changed) {
    m_changed = changed;
  }
  
  /**
    * @returns the scene's ambient color.
    */
  inline Colord ambient() const {
    return m_ambient;
  }
  
  /**
    * Sets the scene's ambient light color.
    * 
    * <table><tr>
    * <td>@image html scene_ambient_red.png</td>
    * <td>@image html scene_ambient_orange.png</td>
    * <td>@image html scene_ambient_yellow.png</td>
    * <td>@image html scene_ambient_green.png</td>
    * <td>@image html scene_ambient_blue.png</td>
    * <td>@image html scene_ambient_indigo.png</td>
    * <td>@image html scene_ambient_violet.png</td>
    * </tr></table>
    */
  inline void setAmbient(const Colord& ambient) {
    m_ambient = ambient;
  }
  
  /**
    * @returns the scene's background color.
    */
  inline const Colord& background() const {
    return m_background;
  }
  
  /**
    * Sets the scene's background light color.
    * 
    * <table><tr>
    * <td>@image html scene_background_red.png</td>
    * <td>@image html scene_background_orange.png</td>
    * <td>@image html scene_background_yellow.png</td>
    * <td>@image html scene_background_green.png</td>
    * <td>@image html scene_background_blue.png</td>
    * <td>@image html scene_background_indigo.png</td>
    * <td>@image html scene_background_violet.png</td>
    * </tr></table>
    */
  inline void setBackground(const Colord& background) {
    m_background = background;
  }
  
  /**
    * @returns the scene's active camera.
    */
  Camera* activeCamera() const;
  virtual bool canHaveChild(Element* child) const;
  
private:
  void findReferences(Element* root, QMap<QString, Element*>& references);
  
  bool m_changed;
  Colord m_ambient;
  Colord m_background;
};
