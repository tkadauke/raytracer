#pragma once
#include <memory>

#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

class Element;
class AbstractParameterWidget;
class QVBoxLayout;

class PropertyEditorWidget : public QWidget {
  Q_OBJECT

public:
  explicit PropertyEditorWidget(Element* root, QWidget* parent = nullptr);
  ~PropertyEditorWidget();

  void setRoot(Element* root);
  void setReadOnlyProperties(const QString& title, const QVector<QPair<QString, QString>>& rows);

  virtual QSize sizeHint() const;

signals:
  void changed(Element*);

public slots:
  void elementChanged(const QString& propertyName, const QVariant& value);
  void setElement(Element* element);
  void update();

private:
  void initLayout();
  void addParameterWidget(AbstractParameterWidget* widget);
  void addParameterWidgets();
  void addParametersForClass(const QMetaObject* klass);
  void clearParameterWidgets();
  void clearReadOnlyWidgets();

  void addParameter(const QString& name);
  void rebuildEditorLater();
  QVBoxLayout* layoutForGroup(const QString& groupName);
  bool isType(const QString& actual, const char* qt5Name, const char* qt6Name) const;

  struct Private;
  std::unique_ptr<Private> p;
};
