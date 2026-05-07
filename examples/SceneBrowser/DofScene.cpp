#include "SceneFactory.h"

#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/lights/PointLight.h"
#include "raytracer/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/textures/ConstantColorTexture.h"

using namespace raytracer;
using namespace render;
using namespace render;

// DOF demo scene built around the canonical "three coloured spheres at
// distinct depths" arrangement that makes depth-of-field visible at a
// glance:
//
//   red sphere   z = -3.5  (in front of focal plane)
//   green sphere z =  0    (at the focal plane @ default focalDistance=5)
//   blue sphere  z = +3.5  (behind the focal plane)
//
// To see the effect, pick **ThinLensCamera** from the camera dropdown in
// SceneBrowser's sidebar (it'll auto-load the ThinLensCameraParameter
// editor), then drag the aperture radius up. With aperture=0.2+ you'll
// see clear blur on the red and blue spheres while the green stays sharp.
//
// **Bump samples_per_pixel high** (≥ 16, ideally 64) — DOF is a sampling
// effect and a 1-sample render looks like a shifted pinhole.
class DofScene : public Scene {
public:
  DofScene();

private:
  std::shared_ptr<MatteMaterial> m_red, m_green, m_blue, m_grey;
};

DofScene::DofScene()
  : Scene()
{
  m_red   = std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord(1.0, 0.2, 0.2)));
  m_green = std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord(0.2, 1.0, 0.2)));
  m_blue  = std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord(0.2, 0.2, 1.0)));
  m_grey  = std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord(0.7, 0.7, 0.7)));

  setAmbient(Colord(0.4, 0.4, 0.4));

  auto front = std::make_shared<Sphere>(Vector3d(-2, -1, -3.5), 0.6);
  front->setMaterial(m_red);

  auto middle = std::make_shared<Sphere>(Vector3d(0, -1, 0), 0.6);
  middle->setMaterial(m_green);

  auto back = std::make_shared<Sphere>(Vector3d(2, -1, 3.5), 0.6);
  back->setMaterial(m_blue);

  // Plane at y=-1 sloping back is the floor — gives the bokeh blur a
  // continuous surface to fall onto so the effect reads instantly.
  auto floor = std::make_shared<Plane>(Vector3d(0, -1, 0), 0);
  floor->setMaterial(m_grey);

  add(front);
  add(middle);
  add(back);
  add(floor);

  addLight(std::make_shared<DirectionalLight>(Vector3d(-0.5, -1, -0.5), Colord(0.8, 0.8, 0.8)));
}

static bool dummy = SceneFactory::self().registerClass<DofScene>("DOF (Three Spheres)");
