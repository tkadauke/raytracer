#pragma once

#include <memory>

#include "engine/graph/RenderGraphTypes.h"
#include "world/objects/Element.h"
#include "world/animation/Timeline.h"
#include "core/Color.h"

class Camera;

namespace render {
  class Scene;
}

class Scene : public Element {
  Q_OBJECT
  Q_PROPERTY(Colord ambient READ ambient WRITE setAmbient)
  Q_PROPERTY(Colord background READ background WRITE setBackground)

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
  std::shared_ptr<render::Scene> toRaytracerScene() const;

  /**
    * Reads this scene from its JSON representation, including the optional
    * top-level `animation` block.
    */
  void read(const QJsonObject& json);

  /**
    * Writes this scene to its JSON representation, including the optional
    * top-level `animation` block.
    */
  void write(QJsonObject& json);

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
    * @returns the scene's animation timeline, or `nullptr` when the scene is
    *   static.
    */
  const world::Timeline* animation() const;

  /**
    * Replaces the scene's animation timeline.
    */
  void setAnimation(std::unique_ptr<world::Timeline> animation);

  /**
    * @returns true if this scene owns an animation timeline.
    */
  bool hasAnimation() const;

  /**
    * @returns the scene's saved render intent. If the scene JSON did not carry
    *   a `renderIntent` block, this is the default intent.
    */
  const engine::graph::RenderIntent& renderIntent() const;

  /**
    * Replaces the scene's saved render intent and marks it present for JSON
    * output.
    */
  void setRenderIntent(engine::graph::RenderIntent intent);

  /**
    * Removes the saved render intent so JSON output omits the top-level block.
    */
  void clearRenderIntent();

  /**
    * @returns true if the scene JSON owns an explicit `renderIntent` block.
    */
  bool hasRenderIntent() const;

  /**
    * Applies the scene's animation timeline at @p frame.
    *
    * Static scenes are left unchanged.
    *
    * @throws std::runtime_error if a track cannot be evaluated or applied.
    */
  void evaluateAnimationAtFrame(int frame);

  /**
    * @returns a deep-copied scene with animation evaluated at @p frame.
    *
    * The original authoring scene is not modified.
    *
    * @throws std::runtime_error if a track cannot be evaluated or applied.
    */
  std::unique_ptr<Scene> evaluatedAtFrame(int frame) const;

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
    * <td>@image html scene_ambient_red.png "red"</td>
    * <td>@image html scene_ambient_orange.png "orange"</td>
    * <td>@image html scene_ambient_yellow.png "yellow"</td>
    * <td>@image html scene_ambient_green.png "green"</td>
    * <td>@image html scene_ambient_blue.png "blue"</td>
    * <td>@image html scene_ambient_indigo.png "indigo"</td>
    * <td>@image html scene_ambient_violet.png "violet"</td>
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
    * <td>@image html scene_background_red.png "red"</td>
    * <td>@image html scene_background_orange.png "orange"</td>
    * <td>@image html scene_background_yellow.png "yellow"</td>
    * <td>@image html scene_background_green.png "green"</td>
    * <td>@image html scene_background_blue.png "blue"</td>
    * <td>@image html scene_background_indigo.png "indigo"</td>
    * <td>@image html scene_background_violet.png "violet"</td>
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
  std::unique_ptr<world::Timeline> m_animation;
  engine::graph::RenderIntent m_renderIntent;
  bool m_hasRenderIntent{false};
};
