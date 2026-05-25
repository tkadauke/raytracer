#pragma once
#include <memory>

#include <QJsonObject>
#include <QJsonValue>

#include "world/objects/Transformable.h"

namespace render {
  class Primitive;
  class Scene;
}

/**
  * Organizes scene objects without adding geometry of its own.
  *
  * A Group can contain surfaces, lights, and other groups. Surface and nested
  * group geometry is converted into a render::Composite wrapped in this group's
  * transform; lights are registered with the runtime scene using their global
  * transform.
  */
class Group : public Transformable {
  Q_OBJECT
  Q_PROPERTY(bool visible READ visible WRITE setVisible)

public:
  /**
    * Default constructor.
    */
  explicit Group(Element* parent = nullptr);

  /**
    * @returns this group's local visible flag. During scene conversion, a
    * hidden group suppresses all descendant surfaces, lights, and nested
    * groups. A visible group still preserves each child's own visible flag.
    */
  inline bool visible() const {
    return m_visible;
  }

  /**
    * Sets the group's visibility property.
    */
  inline void setVisible(bool visible) {
    m_visible = visible;
  }

  /**
    * Sets the group's visible flag to true.
    */
  inline void show() {
    setVisible(true);
  }

  /**
    * Sets the group's visible flag to false.
    */
  inline void hide() {
    setVisible(false);
  }

  /**
    * @returns importer/inspection metadata attached to this group.
    */
  inline const QJsonObject& metadata() const {
    return m_metadata;
  }

  /**
    * Replaces importer/inspection metadata attached to this group.
    */
  inline void setMetadata(const QJsonObject& metadata) {
    m_metadata = metadata;
  }

  /**
    * @returns the metadata value for @p key, or undefined when absent.
    */
  inline QJsonValue metadataValue(const QString& key) const {
    return m_metadata.value(key);
  }

  /**
    * Sets a single metadata value. Passing an undefined value removes @p key.
    */
  void setMetadataValue(const QString& key, const QJsonValue& value);

  /**
    * Removes all metadata from this group.
    */
  inline void clearMetadata() {
    m_metadata = QJsonObject();
  }

  /**
    * Reads this group from scene JSON, including optional metadata.
    */
  void read(const QJsonObject& json) override;

  /**
    * Writes this group to scene JSON, omitting metadata when it is empty.
    */
  void write(QJsonObject& json) override;

  /**
    * Converts visible child geometry into a transformed runtime composite.
    * Hidden groups return null and do not register descendant lights.
    */
  std::shared_ptr<render::Primitive> toRaytracer(render::Scene* scene) const;
  virtual bool canHaveChild(Element* child) const;

private:
  std::shared_ptr<render::Primitive>
  applyTransform(std::shared_ptr<render::Primitive> primitive) const;

  bool m_visible;
  QJsonObject m_metadata;
};
