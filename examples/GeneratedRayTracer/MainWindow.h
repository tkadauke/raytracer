#pragma once

#include <QMainWindow>

class QDockWidget;

class PropertyEditorWidget;
class MaterialDisplayWidget;
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
  
signals:
  void selectionChanged(Element* element);
  void currentElementChanged();
  
private slots:
  void elementSelected(const QModelIndex& current, const QModelIndex& previous);
  void elementChanged(Element*);
  void updateWindowModified();
  void updateMaterialWidget();
  
  void newFile();
  void openFile();
  void saveFile();
  void saveFileAs();
  
  void addBox();
  void addSphere();
  
  void addMatteMaterial();
  void addPhongMaterial();
  void addTransparentMaterial();
  void addReflectiveMaterial();
  
  void addConstantColorTexture();
  void addCheckerBoardTexture();
  
  void deleteElement();
  
  void about();
  void help();

private:
  QDockWidget* createPropertyEditor();
  QDockWidget* createElementSelector();
  QDockWidget* createMaterialDisplay();

  void createActions();
  void createMenus();
  
  void redraw();
  
  bool maybeSave();
  
  template<class T>
  void add();

  QString m_fileName;

  Display* m_display;
  MaterialDisplayWidget* m_materialDisplay;
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
  QAction* m_addReflectiveMaterialAct;
  
  QAction* m_addConstantColorTextureAct;
  QAction* m_addCheckerBoardTextureAct;
  
  QAction* m_deleteElementAct;
  
  QAction* m_aboutAct;
  QAction* m_helpAct;
};
