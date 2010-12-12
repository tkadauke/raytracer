#ifndef SPHERICAL_CAMERA_PARAMETER_WIDGET_H
#define SPHERICAL_CAMERA_PARAMETER_WIDGET_H

#include <QWidget>

class SphericalCameraParameterWidget : public QWidget {
  Q_OBJECT
  
public:
  SphericalCameraParameterWidget(QWidget* parent = 0);
  ~SphericalCameraParameterWidget();
  
  int horizontalFieldOfView() const;
  int verticalFieldOfView() const;

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};

#endif
