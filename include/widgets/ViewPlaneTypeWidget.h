#ifndef VIEW_PLANE_TYPE_WIDGET_H
#define VIEW_PLANE_TYPE_WIDGET_H

#include <QWidget>

#include <string>

class ViewPlaneTypeWidget : public QWidget {
  Q_OBJECT
  
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
  Private* p;
};

#endif
