#pragma once
#include <memory>

#include <QObject>

class Element : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString id READ id WRITE setId)
  Q_PROPERTY(QString name READ name WRITE setName)

public:
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

  inline QString displayName() const {
    if (m_name.isEmpty()) {
      return QString("<%1>").arg(metaObject()->className());
    } else {
      return m_name;
    }
  }

  int row() const;

  virtual void read(const QJsonObject& json);
  virtual void write(QJsonObject& json);

  Element* findById(const QString& id);

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

  virtual void leaveParent();
  virtual void joinParent();

protected:
  template<class T, class... Args>
  inline std::shared_ptr<T> make_named(Args&&... args) const {
    auto result = std::make_shared<T>(args...);
    result->setName(name().toStdString());
    return result;
  }

  void addPendingReference(const QString& property, const QString& id);
  void resolveReferences(const QMap<QString, Element*>& elements);

private:
  void writeForClass(const QMetaObject* klass, QJsonObject& json);
  void writeProperty(const QString& name, QJsonObject& json);

  QList<Element*> m_childElements;

  QString m_id;
  QString m_name;
  bool m_generated;

  QList<QPair<QString, QString>> m_pendingReferences;
};
