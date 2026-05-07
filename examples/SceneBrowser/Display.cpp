#include <QVBoxLayout>
#include <QSpacerItem>

#include <QMouseEvent>

#include "Display.h"
#include "engine/raytracer/Raytracer.h"
#include "render/State.h"
#include "render/cameras/CameraFactory.h"
#include "render/cameras/PinholeCamera.h"
#include "SceneFactory.h"
#include "SceneWidget.h"
#include "widgets/ViewPlaneTypeWidget.h"
#include "widgets/CameraTypeWidget.h"
#include "widgets/CameraParameterWidgetFactory.h"
#include "render/viewplanes/ViewPlaneFactory.h"
#include "core/math/HitPointInterval.h"

using namespace std;
using namespace render;
using namespace render;

Display::Display()
  : QtDisplay(nullptr, std::make_shared<engine::raytracer::Raytracer>(nullptr)),
    m_camera(std::make_shared<PinholeCamera>()),
    m_cameraParameter(nullptr)
{
  m_sidebar = new QWidget(nullptr, Qt::Drawer);
  m_sidebar->show();
  m_verticalLayout = new QVBoxLayout(m_sidebar);
  m_verticalLayout->setContentsMargins(6, 6, 6, 6);
  
  m_scene = new SceneWidget(m_sidebar);
  m_verticalLayout->addWidget(m_scene);

  m_viewPlaneType = new ViewPlaneTypeWidget(m_sidebar);
  m_verticalLayout->addWidget(m_viewPlaneType);
  
  m_cameraType = new CameraTypeWidget(m_sidebar);
  m_verticalLayout->addWidget(m_cameraType);
  
  auto verticalSpacer = new QSpacerItem(20, 186, QSizePolicy::Minimum, QSizePolicy::Expanding);
  m_verticalLayout->addItem(verticalSpacer);
  
  m_engine->setCamera(m_camera);
  m_engine->setScene(SceneFactory::self().createShared("Glass Boxes"));
  m_camera->setViewPlane(render::ViewPlaneFactory::self().createShared(m_viewPlaneType->type()));
  connect(m_scene, SIGNAL(changed()), this, SLOT(sceneChanged()));
  connect(m_viewPlaneType, SIGNAL(changed()), this, SLOT(viewPlaneTypeChanged()));
  connect(m_cameraType, SIGNAL(changed()), this, SLOT(cameraTypeChanged()));
}

Display::~Display() {
  delete m_sidebar;
}

void Display::sceneChanged() {
  stop();
  m_engine->setScene(SceneFactory::self().createShared(m_scene->sceneName()));
  render();
}

void Display::viewPlaneTypeChanged() {
  m_camera->setViewPlane(render::ViewPlaneFactory::self().createShared(m_viewPlaneType->type()));
  render();
}

void Display::cameraTypeChanged() {
  stop();
  m_camera = CameraFactory::self().createShared(m_cameraType->type());
  m_engine->setCamera(m_camera);

  if (m_cameraParameter) {
    delete m_cameraParameter;
  }

  // m_cameraParameter is a Qt widget — Qt's parent/child hierarchy will
  // own it once it's added to the layout, so we release the unique_ptr at
  // the boundary.
  m_cameraParameter = CameraParameterWidgetFactory::self().create(m_cameraType->type()).release();
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

  // Ctrl-click ray-state probe is raytracer-specific; skip if the
  // engine isn't a Raytracer. SceneBrowser today always uses the
  // Raytracer engine, but the base widget is engine-agnostic.
  auto rt = std::dynamic_pointer_cast<engine::raytracer::Raytracer>(m_engine);
  if (!rt) return;

  Rayd ray = m_camera->rayForPixel(event->pos().x(), event->pos().y());
  if (ray.direction().isDefined()) {
    auto state = rt->rayState(ray);

    cout << state.hitPoint.primitive() << " - " << state.hitPoint << endl;
  }
}

