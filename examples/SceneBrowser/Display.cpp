#include <QVBoxLayout>
#include <QSpacerItem>

#include <QMouseEvent>

#include "Display.h"
#include "Raytracer.h"
#include "cameras/CameraFactory.h"
#include "cameras/PinholeCamera.h"
#include "SceneFactory.h"
#include "SceneWidget.h"
#include "widgets/ViewPlaneTypeWidget.h"
#include "widgets/CameraTypeWidget.h"
#include "widgets/CameraParameterWidgetFactory.h"
#include "viewplanes/ViewPlaneFactory.h"
#include "math/HitPointInterval.h"

using namespace std;

Display::Display()
  : QtDisplay(new Raytracer(0)), m_camera(new PinholeCamera), m_cameraParameter(0)
{
  m_sidebar = new QWidget(this, Qt::Drawer);
  m_verticalLayout = new QVBoxLayout(m_sidebar);
  m_verticalLayout->setContentsMargins(6, 6, 6, 6);
  
  m_scene = new SceneWidget(m_sidebar);
  m_verticalLayout->addWidget(m_scene);

  m_viewPlaneType = new ViewPlaneTypeWidget(m_sidebar);
  m_verticalLayout->addWidget(m_viewPlaneType);
  
  m_cameraType = new CameraTypeWidget(m_sidebar);
  m_verticalLayout->addWidget(m_cameraType);
  
  QSpacerItem* verticalSpacer = new QSpacerItem(20, 186, QSizePolicy::Minimum, QSizePolicy::Expanding);
  m_verticalLayout->addItem(verticalSpacer);
  
  m_raytracer->setCamera(m_camera);
  m_raytracer->setScene(SceneFactory::self().create("Glass Boxes"));
  m_camera->setViewPlane(ViewPlaneFactory::self().create(m_viewPlaneType->type()));
  connect(m_scene, SIGNAL(changed()), this, SLOT(sceneChanged()));
  connect(m_viewPlaneType, SIGNAL(changed()), this, SLOT(viewPlaneTypeChanged()));
  connect(m_cameraType, SIGNAL(changed()), this, SLOT(cameraTypeChanged()));
}

Display::~Display() {
  delete m_sidebar;
}

void Display::sceneChanged() {
  stop();
  m_raytracer->setScene(SceneFactory::self().create(m_scene->sceneName()));
  render();
}

void Display::viewPlaneTypeChanged() {
  m_camera->setViewPlane(ViewPlaneFactory::self().create(m_viewPlaneType->type()));
  render();
}

void Display::cameraTypeChanged() {
  stop();
  m_camera = CameraFactory::self().create(m_cameraType->type());
  m_raytracer->setCamera(m_camera);
  
  if (m_cameraParameter) {
    delete m_cameraParameter;
  }

  m_cameraParameter = CameraParameterWidgetFactory::self().create(m_cameraType->type());
  if (m_cameraParameter) {
    m_verticalLayout->addWidget(m_cameraParameter);
    connect(m_cameraParameter, SIGNAL(changed()), this, SLOT(cameraParameterChanged()));
  }
  
  viewPlaneTypeChanged();
}

void Display::cameraParameterChanged() {
  stop();
  m_cameraParameter->applyTo(m_camera);
  render();
}

void Display::mousePressEvent(QMouseEvent* event) {
  QtDisplay::mousePressEvent(event);
  
  Ray ray = m_camera->rayForPixel(event->pos().x(), event->pos().y());
  if (ray.direction().isDefined()) {
    Primitive* primitive = m_raytracer->primitiveForRay(ray);
  
    cout << primitive;
  
    if (primitive) {
      HitPointInterval hitPoints;
      primitive->intersect(ray, hitPoints);
      cout << " - " << endl;
      for (int i = 0; i != hitPoints.points().size(); ++i) {
        cout << (hitPoints.points()[i].in ? "IN" : "OUT") << ": " << hitPoints.points()[i].point << endl;
      }
    }
  
    cout << endl;
  }
}

#include "Display.moc"
