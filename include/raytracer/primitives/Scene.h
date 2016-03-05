#pragma once

#include "raytracer/primitives/Composite.h"
#include "core/Color.h"

namespace raytracer {
  class Light;

  class Scene : public Composite {
  public:
    typedef std::list<Light*> Lights;

    /**
      * Default constructor. Constructs an empty scene with a white ambient
      * color and a white background color.
      */
    inline Scene()
      : m_ambient(Colord::white()),
        m_background(Colord::white())
    {
    }

    /**
      * Constructs an empty scene with the specified ambient color and a white
      * background color.
      */
    inline Scene(const Colord& ambient)
      : m_ambient(ambient),
        m_background(Colord::white())
    {
    }

    ~Scene();

    /**
      * Adds light to the scene.
      */
    inline void addLight(Light* light) {
      m_lights.push_back(light);
    }
    
    /**
      * @returns a list of all lights in the scene.
      */
    inline const Lights& lights() const {
      return m_lights;
    }
    
    /**
      * @returns the scene's ambient color.
      */
    inline const Colord& ambient() const {
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
    
  private:
    Lights m_lights;
    Colord m_ambient;
    Colord m_background;
  };
}
