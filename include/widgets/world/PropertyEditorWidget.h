#pragma once

#include <QWidget>

class Element;
class AbstractParameterWidget;

class PropertyEditorWidget : public QWidget {
  Q_OBJECT
  
public:
  PropertyEditorWidget(Element* root, QWidget* parent = nullptr);
  ~PropertyEditorWidget();
  
  void setRoot(Element* root);
  
  virtual QSize sizeHint() const;

signals:
  void changed(Element*);

public slots:
  void elementChanged(const QString& propertyName, const QVariant& value);
  void setElement(Element* element);

private:
  void initLayout();
  void addParameterWidget(AbstractParameterWidget* widget);
  void addParameterWidgets();
  void addParametersForClass(const QMetaObject* klass);
  void clearParameterWidgets();

  struct Private;
  std::unique_ptr<Private> p;
};
