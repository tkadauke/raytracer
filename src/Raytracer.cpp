#include "Raytracer.h"
#include "math/Vector.h"
#include "math/Ray.h"
#include "surfaces/Scene.h"
#include "Buffer.h"
#include "math/HitPoint.h"
#include "math/HitPointInterval.h"
#include "materials/Material.h"
#include "math/Matrix.h"
#include "math/Rect.h"
#include "math/IntegerDecomposition.h"
#include "cameras/Camera.h"
#include "core/Exception.h"

#include "cameras/PinholeCamera.h"

#include <QThread>

#include <vector>
#include <cmath>

using namespace std;

namespace {
  class RenderThread : public QThread {
  public:
    RenderThread(Raytracer* rt, Camera* c, Buffer<unsigned int>& b, const Rect& r)
      : QThread(), raytracer(rt), camera(c), buffer(b), rect(r) {}

    virtual void run() {
      camera->render(raytracer, buffer, rect);
    }

    Raytracer* raytracer;
    Camera* camera;
    Buffer<unsigned int>& buffer;
    Rect rect;
  };
}

struct Raytracer::Private {
  Private()
    : numberOfThreads(24) {}
  
  vector<RenderThread*> threads;
  int numberOfThreads;
};

Raytracer::Raytracer(Scene* scene)
  : m_scene(scene), p(new Private)
{
  m_camera = new PinholeCamera;
}

Raytracer::Raytracer(Camera* camera, Scene* scene)
  : m_camera(camera), m_scene(scene), p(new Private)
{
}

Raytracer::~Raytracer() {
  delete p;
}

void Raytracer::render(Buffer<unsigned int>& buffer) {
  m_camera->uncancel();
  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
  
  p->threads.clear();
  IntegerDecomposition d(p->numberOfThreads);
  for (int horz = 0; horz != d.second(); ++horz) {
    for (int vert = 0; vert != d.first(); ++vert) {
      p->threads.push_back(new RenderThread(this, m_camera, buffer, Rect(
        floor(double(buffer.width()) / d.second() * horz),
        floor(double(buffer.height()) / d.first() * vert),
        ceil(double(buffer.width()) / d.second()),
        ceil(double(buffer.height()) / d.first())
      )));
    }
  }
  
  for (vector<RenderThread*>::iterator i = p->threads.begin(); i != p->threads.end(); ++i)
    (*i)->start();
  
  for (vector<RenderThread*>::iterator i = p->threads.begin(); i != p->threads.end(); ++i) {
    (*i)->wait();
    delete *i;
  }
}

Colord Raytracer::rayColor(const Ray& ray, int recursionDepth) {
  if (recursionDepth == 5) {
    return m_scene->ambient();
  }
  
  HitPointInterval hitPoints;
  
  Surface* surface = m_scene->intersect(ray, hitPoints);
  if (surface) {
    HitPoint hitPoint = hitPoints.minWithPositiveDistance();
    
    Colord color = surface->material()->shade(this, ray, hitPoint, recursionDepth);
    
    return color;
  } else {
    return m_scene->ambient();
  }
}

void Raytracer::setCamera(Camera* camera) {
  if (m_camera)
    delete m_camera;
  m_camera = camera;
}

void Raytracer::cancel() {
  m_camera->cancel();
}
