#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
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
#include "core/ScopeExit.h"

#include "raytracer/cameras/PinholeCamera.h"

#include <QThread>

#include <vector>
#include <cmath>

#include <iostream>

using namespace std;
using namespace raytracer;

namespace {
  class RenderThread : public QThread {
  public:
    inline RenderThread(std::shared_ptr<Raytracer> rt, std::shared_ptr<Camera> c, Buffer<unsigned int>& b, const Recti& r)
      : QThread(),
        raytracer(rt),
        camera(c),
        buffer(b),
        rect(r)
    {
    }

    inline virtual void run() {
      camera->render(raytracer, buffer, rect);
    }

    std::shared_ptr<Raytracer> raytracer;
    std::shared_ptr<Camera> camera;
    Buffer<unsigned int>& buffer;
    Recti rect;
  };
}

struct Raytracer::Private {
  inline Private()
    : numberOfThreads(24),
      maximumRecursionDepth(5)
  {
  }
  
  vector<RenderThread*> threads;
  int numberOfThreads;
  int maximumRecursionDepth;
};

Raytracer::Raytracer(Scene* scene)
  : m_camera(std::make_shared<PinholeCamera>()),
    m_scene(scene),
    p(std::make_unique<Private>())
{
}

Raytracer::Raytracer(std::shared_ptr<Camera> camera, Scene* scene)
  : m_camera(camera),
    m_scene(scene),
    p(std::make_unique<Private>())
{
}

Raytracer::~Raytracer() {
}

void Raytracer::render(Buffer<unsigned int>& buffer) {
  if (!m_scene) {
    buffer.clear();
    return;
  }
  
  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
  
  p->threads.clear();
  IntegerDecomposition d(p->numberOfThreads);
  for (int horz = 0; horz != d.second(); ++horz) {
    for (int vert = 0; vert != d.first(); ++vert) {
      p->threads.push_back(new RenderThread(shared_from_this(), m_camera, buffer, Recti(
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

const Primitive* Raytracer::primitiveForRay(const Rayd& ray) const {
  return rayState(ray).hitPoint.primitive();
}

State Raytracer::rayState(const Rayd& ray) const {
  State state;
  state.startTrace();
  rayColor(ray, state);
  return state;
}

Colord Raytracer::rayColor(const Rayd& ray, State& state) const {
  state.recurseIn();
  ScopeExit sx([&] { state.recurseOut(); });
  
  if (state.recursionDepth == p->maximumRecursionDepth) {
    state.recordEvent("Raytracer: maximum recursion depth reached, returning black");
    return Colord::black();
  }
  
  HitPointInterval hitPoints;
  
  auto primitive = m_scene->intersect(ray, hitPoints, state);
  if (primitive) {
    auto hitPoint = hitPoints.minWithPositiveDistance();
    
    if (state.recursionDepth == 1) {
      state.hitPoint = hitPoint;
    }
    
    if (primitive->material()) {
      state.recordEvent("Raytracer: shading material");
      return primitive->material()->shade(this, ray, hitPoint, state);
    } else {
      state.recordEvent("Raytracer: no material found, returning black");
      return Colord::black();
    }
  } else {
    state.recordEvent("Raytracer: Nothing hit, returning background color");
    return m_scene->background();
  }
}

void Raytracer::cancel() {
  m_camera->cancel();
}

void Raytracer::uncancel() {
  m_camera->uncancel();
}

void Raytracer::setMaximumRecursionDepth(int depth) {
  p->maximumRecursionDepth = depth;
}
