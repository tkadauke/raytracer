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

  void addIntersection();
  
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

  void reorder();

private:
  QDockWidget* createPropertyEditor();
  QDockWidget* createElementSelector();
  QDockWidget* createPreviewDisplay();

  void createActions();
  void createMenus();
  
  bool maybeSave();
  
  void redraw();
  
  template<class T>
  void add();

  struct Private;
  std::unique_ptr<Private> p;
};
