#include "raytracer/Raytracer.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"
#include "raytracer/primitives/Scene.h"
#include "core/Buffer.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "raytracer/materials/Material.h"
#include "core/math/Matrix.h"
#include "core/math/Rect.h"
#include "core/math/IntegerDecomposition.h"
#include "raytracer/cameras/Camera.h"
#include "core/Exception.h"

#include "raytracer/cameras/PinholeCamera.h"

#include <QThread>

#include <vector>
#include <cmath>

using namespace std;
using namespace raytracer;

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
    : numberOfThreads(8) {}
  
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
  
  for (auto& thread : p->threads)
    thread->start();
  
  for (auto& thread : p->threads) {
    thread->wait();
    delete thread;
  }
}

Primitive* Raytracer::primitiveForRay(const Ray& ray) {
  HitPointInterval hitPoints;
  return m_scene->intersect(ray, hitPoints);
}

Colord Raytracer::rayColor(const Ray& ray, int recursionDepth) {
  if (recursionDepth == 15) {
    return m_scene->ambient();
  }
  
  HitPointInterval hitPoints;
  
  auto primitive = m_scene->intersect(ray, hitPoints);
  if (primitive) {
    auto hitPoint = hitPoints.minWithPositiveDistance();
    
    return primitive->material()->shade(this, ray, hitPoint, recursionDepth);
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

void Raytracer::uncancel() {
  m_camera->uncancel();
}
