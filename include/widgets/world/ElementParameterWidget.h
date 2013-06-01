#ifndef ELEMENT_PARAMETER_WIDGET_H
#define ELEMENT_PARAMETER_WIDGET_H

#include <QWidget>

class Element;

class ElementParameterWidget : public QWidget {
  Q_OBJECT
  
public:
  ElementParameterWidget(QWidget* parent = 0);
  ~ElementParameterWidget();
  
  QString name() const;
  void setName(const QString& name);

  virtual void applyTo(Element* object);
  virtual void getFrom(Element* object);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};

#endif
