#ifndef RAYTRACER_TEST_HELPER_H
#define RAYTRACER_TEST_HELPER_H

#include "gtest/gtest.h"
#include "Vector.h"
#include "Buffer.h"
#include "Color.h"

class Surface;
class Scene;
class Raytracer;
class Camera;
class Sphere;
class Material;

namespace testing {
  class RaytracerFunctionalTest : public ::testing::Test {
  public:
    RaytracerFunctionalTest();
    
  protected:
    virtual void SetUp();
    virtual void TearDown();
    
    void add(Surface* surface);
    Camera* camera();
    void setCamera(Camera* camera);
    void setCamera(const Vector3d& position, const Vector3d& lookAt);
    void setView(const Vector3d& position, const Vector3d& lookAt);
    void render();
    
    bool colorPresent(const Colord& color);
    int colorCount(const Colord& color);
    void show();
    
    Material* redDiffuse();
    Sphere* centeredSphere();
    Sphere* displacedSphere();
    void lookAtOrigin();
    void lookAway();
    void goFarAway();
    bool objectVisible();
    int objectSize();
  
  private:
    Scene* m_scene;
    Camera* m_camera;
    Raytracer* m_raytracer;
    Buffer m_buffer;
  };
}

#endif
