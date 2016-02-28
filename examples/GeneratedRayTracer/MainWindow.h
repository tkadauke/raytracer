#pragma once

#include <QMainWindow>

class QDockWidget;

class PropertyEditorWidget;
class PreviewDisplayWidget;
class Display;
class SceneModel;

class Element;
class Scene;

class RenderWindow;

class MainWindow : public QMainWindow {
  Q_OBJECT;

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
  void updatePreviewWidget();
  
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

  void addPinholeCamera();
  void addFishEyeCamera();
  void addOrthographicCamera();
  void addSphericalCamera();
  
  void deleteElement();
  
  void render();
  
  void about();
  void help();

private:
  QDockWidget* createPropertyEditor();
  QDockWidget* createElementSelector();
  QDockWidget* createPreviewDisplay();

  void createActions();
  void createMenus();
  
  void redraw();
  
  bool maybeSave();
  
  template<class T>
  void add();

  QString m_fileName;

  Display* m_display;
  PreviewDisplayWidget* m_materialDisplay;
  PropertyEditorWidget* m_propertyEditorWidget;
  SceneModel* m_elementModel;
  
  RenderWindow* m_renderWindow;
  
  Scene* m_scene;
  
  Element* m_currentElement;
  
  QMenu* m_fileMenu;
  QMenu* m_editMenu;
  QMenu* m_renderMenu;
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

  QAction* m_addPinholeCameraAct;
  QAction* m_addFishEyeCameraAct;
  QAction* m_addOrthographicCameraAct;
  QAction* m_addSphericalCameraAct;
  
  QAction* m_deleteElementAct;

  QAction* m_renderAct;
  
  QAction* m_aboutAct;
  QAction* m_helpAct;
};
