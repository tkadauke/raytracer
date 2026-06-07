#pragma once
#include <memory>
#include <optional>
#include <vector>

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QVariant>

namespace engine::graph {
  class RenderSceneAnalysis;
}

namespace render {
  class Object;
}

class Element : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString id READ id WRITE setId)
  Q_PROPERTY(QString name READ name WRITE setName)

public:
  enum class AnimationPropertyType {
    Unsupported,
    Double,
    Integer,
    Vector3,
    Color,
    Boolean,
    String
  };

  struct AnimationPropertyInfo {
    AnimationPropertyType type{AnimationPropertyType::Unsupported};
    QString typeName;
    bool writable{false};
  };

  explicit Element(Element* parent = nullptr);
  virtual ~Element();

  inline const QString& id() const {
    return m_id;
  }

  inline void setId(const QString& id) {
    m_id = id;
  }

  inline const QString& name() const {
    return m_name;
  }

  inline void setName(const QString& name) {
    m_name = name;
  }

  inline bool isGenerated() const {
    return m_generated;
  }

  inline void setGenerated(bool generated) {
    m_generated = generated;
  }

  virtual bool displayInSceneModel() const;
  virtual bool isPropertyVisible(const QString& propertyName) const;
  virtual QString propertyDisplayName(const QString& propertyName) const;
  virtual QString propertyDescription(const QString& propertyName) const;
  virtual QString propertyGroup(const QString& propertyName) const;
  virtual QStringList propertyChoices(const QString& propertyName) const;
  virtual QList<int> propertyIntChoices(const QString& propertyName) const;
  virtual std::optional<QPair<int, int>> propertyIntRange(const QString& propertyName) const;
  virtual std::optional<QPair<double, double>>
  propertyDoubleRange(const QString& propertyName) const;
  virtual std::optional<double> propertyDoubleStep(const QString& propertyName) const;
  virtual QString propertyChoiceDisplayName(const QString& propertyName,
                                            const QString& choice) const;
  virtual bool rebuildPropertyEditorAfterChange(const QString& propertyName) const;
  virtual void propertyEdited(const QString& propertyName);
  /**
    * Describes how @p propertyName participates in world animation.
    *
    * The default implementation exposes writable direct `Q_PROPERTY`s whose
    * value types are understood by `world::AnimationTrack`. Subclasses can
    * override this to expose dynamic properties, such as importer-defined
    * source parameters.
    */
  virtual std::optional<AnimationPropertyInfo>
  animationPropertyInfo(const QString& propertyName) const;

  /**
    * Applies a sampled animation value.
    *
    * The default implementation writes through `QObject::setProperty()` and
    * then calls `propertyEdited()` so element-specific side effects stay in
    * one place.
    */
  virtual bool setAnimatedProperty(const QString& propertyName, const QVariant& value);

  inline QString displayName() const {
    if (m_name.isEmpty()) {
      return QString("<%1>").arg(metaObject()->className());
    } else {
      return m_name;
    }
  }

  /**
    * @returns tag metadata used by render graph scene selectors.
    *
    * Both `tag: "name"` and `tags: ["a", "b"]` JSON forms are accepted so
    * importers can preserve their native shape while graph compilation sees a
    * stable selector vocabulary.
    */
  std::vector<QString> renderGraphTags() const;

  /**
    * @returns layer metadata used by render graph scene selectors.
    *
    * String-valued `layer`, `layerName`, and `layers` metadata are accepted.
    * Integer `layerIndex` metadata is exposed as its decimal string.
    */
  std::vector<QString> renderGraphLayers() const;

  int row() const;

  virtual void read(const QJsonObject& json);
  virtual void write(QJsonObject& json);

  /**
    * @returns importer/inspection metadata attached to this element.
    */
  inline const QJsonObject& metadata() const {
    return m_metadata;
  }

  /**
    * Replaces importer/inspection metadata attached to this element.
    *
    * Metadata is intentionally opaque to the renderer. Importers can store
    * source provenance, source object IDs, layer names, category tags, or other
    * structured JSON here without introducing format-specific subclasses.
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
    * Removes all metadata from this element.
    */
  inline void clearMetadata() {
    m_metadata = QJsonObject();
  }

  Element* findById(const QString& id);
  const Element* findById(const QString& id) const;

  inline Element* parent() const {
    return static_cast<Element*>(QObject::parent());
  }

  inline const QList<Element*> childElements() const {
    return m_childElements;
  }

  virtual bool canHaveChild(Element* child) const;

  /**
    * Adds @p child as the last child of this element. Two overloads:
    *
    * - **unique_ptr** — for callers that own a fresh element (e.g. from
    *   ElementFactory::create()) and want to hand it off; the type signals
    *   the ownership transfer.
    * - **raw pointer** — for re-parenting an element that already lives in
    *   the QObject tree somewhere (e.g. a drag-and-drop move in the model
    *   view); ownership stays with Qt either way.
    *
    * Both forms ultimately reach insertChild, which calls QObject::setParent
    * on the child so Qt's parent/child hierarchy owns the lifetime.
    */
  inline void addChild(std::unique_ptr<Element> child) {
    insertChild(m_childElements.size(), std::move(child));
  }

  inline void addChild(Element* child) {
    insertChild(m_childElements.size(), child);
  }

  inline void insertChild(int index, std::unique_ptr<Element> child) {
    insertChild(index, child.release());
  }

  void insertChild(int index, Element* child);
  void removeChild(int index, bool removeParent = true);
  inline void removeChild(Element* child, bool removeParent = true) {
    removeChild(m_childElements.indexOf(child), removeParent);
  }

  void moveChild(int from, int to);

  void unlink(Element* root);

  /**
    * Adds this element's render-graph-relevant scene facts to @p analysis.
    *
    * The default implementation delegates to child elements. Concrete scene
    * object types add their own facts, such as visible surfaces or lights,
    * before recursing when their visibility permits it.
    */
  virtual void contributeToRenderGraphAnalysis(engine::graph::RenderSceneAnalysis& analysis) const;

  virtual void leaveParent();
  virtual void joinParent();

protected:
  template<class T, class... Args>
  inline std::shared_ptr<T> make_named(Args&&... args) const {
    auto result = std::make_shared<T>(args...);
    result->setName(name().toStdString());
    return result;
  }

  void attachRuntimeAnimationTracks(render::Object& object) const;

  void addPendingReference(const QString& property, const QString& id);
  void resolveReferences(const QMap<QString, Element*>& elements);

private:
  AnimationPropertyType animationPropertyTypeForTypeName(const QString& typeName) const;
  QString humanizePropertyName(const QString& propertyName) const;

  void writeForClass(const QMetaObject* klass, QJsonObject& json);
  void writeProperty(const QString& name, QJsonObject& json);

  QList<Element*> m_childElements;

  QString m_id;
  QString m_name;
  QJsonObject m_metadata;
  bool m_generated;

  QList<QPair<QString, QString>> m_pendingReferences;
};
