#pragma once

#include <QObject>

class Element : public QObject {
  Q_OBJECT;
  Q_PROPERTY(QString id READ id WRITE setId);
  Q_PROPERTY(QString name READ name WRITE setName);
  
public:
  Element(Element* parent = nullptr);
  virtual ~Element();
  
  inline const QString& id() {
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
  
protected:
  void addPendingReference(const QString& property, const QString& id);
  void resolveReferences(const QMap<QString, Element*>& elements);
  
private:
  void unlink(QObject* root);
  
  void writeForClass(const QMetaObject* klass, QJsonObject& json);
  
  QString m_id;
  QString m_name;
  
  QList<QPair<QString, QString>> m_pendingReferences;
};
