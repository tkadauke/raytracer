#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QDockWidget;

class PropertyEditorWidget;
class Display;

class Element;
class Scene;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow();
  
protected:
  void initScene();
  
private slots:
  void elementChanged(Element*);

private:
  Display* m_display;
  PropertyEditorWidget* m_propertyEditorWidget;
  QDockWidget* m_sidebar;
  
  Scene* m_scene;
};

#endif
