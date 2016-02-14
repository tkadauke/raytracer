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
  
  void newFile();
  
  void addBox();
  void addSphere();
  void addMatteMaterial();
  
  void deleteElement();
  
  void about();
  void help();

private:
  QDockWidget* createPropertyEditor();
  QDockWidget* createElementSelector();

  void createActions();
  void createMenus();

  Display* m_display;
  PropertyEditorWidget* m_propertyEditorWidget;
  ElementModel* m_elementModel;
  
  Scene* m_scene;
  
  Element* m_currentElement;
  
  QMenu* m_fileMenu;
  QMenu* m_editMenu;
  QMenu* m_helpMenu;

  QAction* m_newAct;

  QAction* m_addBoxAct;
  QAction* m_addSphereAct;
  QAction* m_addMatteMaterialAct;
  
  QAction* m_deleteElementAct;
  
  QAction* m_aboutAct;
  QAction* m_helpAct;
};
