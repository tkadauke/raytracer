#pragma once

#include <QObject>

class Element : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString name READ name WRITE setName);
  
public:
  Element(Element* parent);
  virtual ~Element();
  
  int row() const;
  
  inline const QString& name() const { return m_name; }
  inline void setName(const QString& name) { m_name = name; }
  
private:
  QString m_name;
};
