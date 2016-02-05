#ifndef PROPERTY_EDITOR_WIDGET_H
#define PROPERTY_EDITOR_WIDGET_H

#include <QWidget>

class Element;
class AbstractParameterWidget;

class PropertyEditorWidget : public QWidget {
  Q_OBJECT
  
public:
  PropertyEditorWidget(QWidget* parent = 0);
  ~PropertyEditorWidget();
  
  virtual QSize sizeHint() const;

signals:
  void changed(Element*);

public slots:
  void elementChanged(const QString& propertyName, const QVariant& value);
  void setElement(Element* element);

private:
  void addParameterWidget(AbstractParameterWidget* widget);
  void addParameterWidgets();
  void addParametersForClass(const QMetaObject* klass);
  void clearParameterWidgets();

  struct Private;
  Private* p;
};

#endif
