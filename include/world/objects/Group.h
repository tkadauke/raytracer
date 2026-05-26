#pragma once
#include <memory>
#include <optional>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "world/objects/Transformable.h"

namespace render {
  class Primitive;
  class Scene;
}

/**
  * Generic JSON metadata keys used by Group helper APIs and import provenance.
  */
namespace GroupMetadata {
  inline QString sourceFormatKey() {
    return QStringLiteral("sourceFormat");
  }

  inline QString sourceIdKey() {
    return QStringLiteral("sourceId");
  }

  inline QString stepIndexKey() {
    return QStringLiteral("stepIndex");
  }

  inline QString layerIndexKey() {
    return QStringLiteral("layerIndex");
  }

  inline QString startTimeKey() {
    return QStringLiteral("startTime");
  }

  inline QString endTimeKey() {
    return QStringLiteral("endTime");
  }

  inline QString labelKey() {
    return QStringLiteral("label");
  }
}

/**
  * Organizes scene objects without adding geometry of its own.
  *
  * A Group can contain surfaces, lights, and other groups. Surface and nested
  * group geometry is converted into a render::Composite wrapped in this group's
  * transform; lights are registered with the runtime scene using their global
  * transform.
  *
  * Scene JSON may use either `"Group"` or `"Collection"` as the element type.
  * Both names create this class; `Collection` is an importer-friendly alias for
  * formats and tools that use collection terminology for hierarchy-only nodes.
  *
  * Groups are authoring hierarchy, not render layers or AOVs. Their `visible`
  * flag decides whether descendant scene objects are converted for rendering;
  * it does not create a separate output pass, mask, or render-graph resource.
  */
class Group : public Transformable {
  Q_OBJECT
  Q_PROPERTY(bool visible READ visible WRITE setVisible)

public:
  /**
    * Creates a visible, empty group with no importer metadata.
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
    *
    * Hiding a group suppresses every descendant surface, light, and nested
    * group during scene conversion. Showing it only re-enables the group node
    * itself; descendants still apply their own `visible` flags.
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
    *
    * Metadata is intentionally opaque to the renderer. Importers can store
    * source object IDs, step indices, layer/frame indices, time intervals,
    * labels, category tags, or other structured JSON here without changing
    * runtime primitive generation.
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
    * @returns a generic ordered step index, or empty when the metadata is
    * absent or is not an integer-valued JSON number.
    */
  std::optional<int> stepIndex() const;

  /**
    * Sets or removes the generic ordered step index metadata.
    */
  void setStepIndex(std::optional<int> index);

  /**
    * @returns a generic layer/frame index, or empty when the metadata is absent
    * or is not an integer-valued JSON number.
    */
  std::optional<int> layerIndex() const;

  /**
    * Sets or removes the generic layer/frame index metadata.
    */
  void setLayerIndex(std::optional<int> index);

  /**
    * @returns the start time for an imported interval/frame, or empty when
    * absent or not numeric. Units are defined by the importer.
    */
  std::optional<double> startTime() const;

  /**
    * @returns the end time for an imported interval/frame, or empty when absent
    * or not numeric. Units are defined by the importer.
    */
  std::optional<double> endTime() const;

  /**
    * Sets or removes the generic time interval metadata. Units are defined by
    * the importer, but start and end use the same unit.
    */
  void setTimeRange(std::optional<double> startTime,
                    std::optional<double> endTime);

  /**
    * @returns a display label for the step/layer/frame, or empty when absent or
    * not a string.
    */
  std::optional<QString> label() const;

  /**
    * Sets or removes the display label metadata.
    */
  void setLabel(const std::optional<QString>& label);

  /**
    * Removes all metadata from this group.
    */
  inline void clearMetadata() {
    m_metadata = QJsonObject();
  }

  /**
    * Reads this group from scene JSON, including optional metadata.
    *
    * The `metadata` member must be a JSON object when present. Unknown metadata
    * keys and value types are preserved as-is.
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

  /**
    * @returns true for surfaces, lights, and other groups. Groups reject
    *   materials, textures, cameras, and other authoring objects because those
    *   belong at scene scope or on a specific surface.
    */
  virtual bool canHaveChild(Element* child) const;

private:
  std::shared_ptr<render::Primitive>
  applyTransform(std::shared_ptr<render::Primitive> primitive) const;

  bool m_visible;
  QJsonObject m_metadata;
};
