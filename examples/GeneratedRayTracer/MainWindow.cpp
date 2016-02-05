#include <QVBoxLayout>
#include <QSpacerItem>
#include <QDockWidget>

#include <QMouseEvent>

#include "MainWindow.h"
#include "Display.h"
#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Primitive.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/Light.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "core/math/HitPointInterval.h"

#include "widgets/world/PropertyEditorWidget.h"

#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

using namespace std;

MainWindow::MainWindow()
  : QMainWindow()
{
  m_display = new Display(this);
  setCentralWidget(m_display);
    
  initScene();
  m_display->setScene(m_scene);
  
  Element* element = static_cast<Element*>(m_scene->children()[0]);

  m_propertyEditorWidget = new PropertyEditorWidget(this);
  m_propertyEditorWidget->setElement(element);

  connect(m_propertyEditorWidget, SIGNAL(changed(Element*)), this, SLOT(elementChanged(Element*)));

  m_sidebar = new QDockWidget("Properties", this);
  m_sidebar->setWidget(m_propertyEditorWidget);
  
  addDockWidget(Qt::LeftDockWidgetArea, m_sidebar);
}

MainWindow::~MainWindow() {
  delete m_sidebar;
}

void MainWindow::initScene() {
  m_scene = new ::Scene(0);
  Sphere* sphere = new Sphere(m_scene);
  sphere->setRadius(1);

  sphere = new Sphere(m_scene);
  sphere->setRadius(1);
  sphere->setPosition(Vector3d(1, 0, 0));
}

void MainWindow::elementChanged(Element* element) {
  m_display->setScene(m_scene);
}

#include "MainWindow.moc"
