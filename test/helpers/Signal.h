#ifndef SIGNAL_H
#define SIGNAL_H

#include <QObject>

class Signal : public QObject {
  Q_OBJECT;
public:
  void call();
  
signals:
  void send();
};

#endif
