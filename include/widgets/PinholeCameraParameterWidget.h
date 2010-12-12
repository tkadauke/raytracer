#ifndef PINHOLE_CAMERA_PARAMETER_WIDGET_H
#define PINHOLE_CAMERA_PARAMETER_WIDGET_H

#include <QWidget>

class PinholeCameraParameterWidget : public QWidget {
  Q_OBJECT
  
public:
  PinholeCameraParameterWidget(QWidget* parent = 0);
  ~PinholeCameraParameterWidget();
  
  double distance() const;

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};

#endif
