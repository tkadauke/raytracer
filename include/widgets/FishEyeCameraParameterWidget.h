#ifndef FISH_EYE_CAMERA_PARAMETER_WIDGET_H
#define FISH_EYE_CAMERA_PARAMETER_WIDGET_H

#include <QWidget>

class FishEyeCameraParameterWidget : public QWidget {
  Q_OBJECT
  
public:
  FishEyeCameraParameterWidget(QWidget* parent = 0);
  ~FishEyeCameraParameterWidget();
  
  int fieldOfView() const;

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};

#endif
