#pragma once

#include <QMainWindow>

class QDockWidget;

class PropertyEditorWidget;
class Display;
class ElementModel;

class Element;
class Scene;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();
  
protected:
  void initScene();
  
private slots:
  void elementSelected(const QModelIndex& current, const QModelIndex& previous);
  void elementChanged(Element*);

private:
  QDockWidget* createPropertyEditor();
  QDockWidget* createElementSelector();
  
  Display* m_display;
  PropertyEditorWidget* m_propertyEditorWidget;
  ElementModel* m_elementModel;
  
  Scene* m_scene;
};
