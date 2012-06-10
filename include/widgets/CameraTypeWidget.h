#ifndef CAMERA_TYPE_WIDGET_H
#define CAMERA_TYPE_WIDGET_H

#include <QWidget>

#include <string>

class CameraTypeWidget : public QWidget {
  Q_OBJECT
  
public:
  CameraTypeWidget(QWidget* parent = 0);
  ~CameraTypeWidget();
  
  std::string type() const;

signals:
  void changed();

private slots:
  void typeChanged();

private:
  struct Private;
  Private* p;
};

#endif
