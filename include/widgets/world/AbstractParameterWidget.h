#ifndef ABSTRACT_PARAMETER_WIDGET_H
#define ABSTRACT_PARAMETER_WIDGET_H

#include <QWidget>

class Element;

class AbstractParameterWidget : public QWidget {
  Q_OBJECT
  
public:
  AbstractParameterWidget(QWidget* parent = nullptr);
  ~AbstractParameterWidget();
  
  void setElement(Element* element);
  virtual void setParameterName(const QString& name);
  
  virtual const QVariant value() = 0;
  virtual void setValue(const QVariant& value) = 0;

signals:
  void changed(const QString&, const QVariant&);

protected slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};

#endif
