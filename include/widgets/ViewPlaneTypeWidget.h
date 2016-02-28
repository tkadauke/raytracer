#pragma once

#include <QWidget>

#include <string>

class ViewPlaneTypeWidget : public QWidget {
  Q_OBJECT;
  
public:
  ViewPlaneTypeWidget(QWidget* parent = nullptr);
  ~ViewPlaneTypeWidget();
  
  std::string type() const;

signals:
  void changed();

private slots:
  void typeChanged();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
