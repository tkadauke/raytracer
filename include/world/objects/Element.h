#ifndef ELEMENT_H
#define ELEMENT_H

#include <QObject>
#include <QString>

class Element : public QObject {
  Q_OBJECT
  
public:
  typedef std::list<Element*> Children;
  
  Element();
  virtual ~Element();
  
  inline const QString& name() const { return m_name; }
  inline void setName(const QString& name) { m_name = name; }
  
  inline void add(Element* child) { m_children.push_back(child); }
  inline const Children& children() const { return m_children; }
  
  Q_PROPERTY(QString name READ name WRITE setName USER true)
  
private:
  QString m_name;
  Children m_children;
};

#endif
