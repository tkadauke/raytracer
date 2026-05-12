#ifndef SLOT_H
#define SLOT_H

#include <QObject>

class Slot : public QObject {
  Q_OBJECT
public:
  Slot();
  bool called();
  
public slots:
  void receive();
  
private:
  bool m_called;
};

#endif
