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
  virtual void closeEvent(QCloseEvent* event);
  
private slots:
  void elementSelected(const QModelIndex& current, const QModelIndex& previous);
  void elementChanged(Element*);
  void updateWindowModified();
  
  void newFile();
  void openFile();
  void saveFile();
  void saveFileAs();
  
  void addBox();
  void addSphere();
  void addMatteMaterial();
  void addPhongMaterial();
  void addTransparentMaterial();
  
  void deleteElement();
  
  void about();
  void help();

private:
  QDockWidget* createPropertyEditor();
  QDockWidget* createElementSelector();

  void createActions();
  void createMenus();
  
  void redraw();
  
  bool maybeSave();
  
  template<class Mat>
  void addMaterial(const QString& name);

  QString m_fileName;

  Display* m_display;
  PropertyEditorWidget* m_propertyEditorWidget;
  ElementModel* m_elementModel;
  
  Scene* m_scene;
  
  Element* m_currentElement;
  
  QMenu* m_fileMenu;
  QMenu* m_editMenu;
  QMenu* m_helpMenu;

  QAction* m_newAct;
  QAction* m_openAct;
  QAction* m_saveAct;
  QAction* m_saveAsAct;

  QAction* m_addBoxAct;
  QAction* m_addSphereAct;
  QAction* m_addMatteMaterialAct;
  QAction* m_addPhongMaterialAct;
  QAction* m_addTransparentMaterialAct;
  
  QAction* m_deleteElementAct;
  
  QAction* m_aboutAct;
  QAction* m_helpAct;
};
