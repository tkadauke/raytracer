#pragma once

#include <QWidget>

#include <string>

class CameraTypeWidget : public QWidget {
  Q_OBJECT;
  
public:
  explicit CameraTypeWidget(QWidget* parent = nullptr);
  ~CameraTypeWidget();
  
  std::string type() const;

signals:
  void changed();

private slots:
  void typeChanged();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
