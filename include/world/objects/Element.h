#pragma once

#include <QObject>

class Element : public QObject {
  Q_OBJECT;
  Q_PROPERTY(QString id READ id WRITE setId);
  Q_PROPERTY(QString name READ name WRITE setName);
  
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
  
  int row() const;
  
  void read(const QJsonObject& json);
  void write(QJsonObject& json);
  
  Element* findById(const QString& id);
  
  inline Element* parent() const {
    return static_cast<Element*>(QObject::parent());
  }
  
  inline const QList<Element*> childElements() const {
    return m_childElements;
  }
  
  virtual bool canHaveChild(Element* child) const;
  
  inline void addChild(Element* child) {
    insertChild(m_childElements.size(), child);
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
  void addPendingReference(const QString& property, const QString& id);
  void resolveReferences(const QMap<QString, Element*>& elements);
  
private:
  void writeForClass(const QMetaObject* klass, QJsonObject& json);
  
  QList<Element*> m_childElements;
  
  QString m_id;
  QString m_name;
  
  QList<QPair<QString, QString>> m_pendingReferences;
};
