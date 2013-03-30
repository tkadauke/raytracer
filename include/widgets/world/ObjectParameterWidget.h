#ifndef OBJECT_PARAMETER_WIDGET_H
#define OBJECT_PARAMETER_WIDGET_H

#include <string>

#include <QWidget>

class Object;

class ObjectParameterWidget : public QWidget {
  Q_OBJECT
  
public:
  ObjectParameterWidget(QWidget* parent = 0);
  ~ObjectParameterWidget();
  
  std::string name() const;
  void setName(const std::string& name);

  virtual void applyTo(Object* object);
  virtual void getFrom(Object* object);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};

#endif
