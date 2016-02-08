#pragma once

#include <QObject>

class Element : public QObject {
  Q_OBJECT
  
public:
  Element(Element* parent);
  virtual ~Element();
  
  int row() const;
  
private:
  QString m_name;
};
