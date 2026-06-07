#include "world/objects/ElementFactory.h"
#include "world/objects/Disk.h"
#include "render/primitives/Disk.h"

Disk::Disk(Element* parent)
    : Surface(parent),
      m_radius(1) {
}

std::shared_ptr<render::Primitive> Disk::toRaytracerPrimitive() const {
  // Local frame: centre at origin, normal along +Y. The Surface
  // base wraps this in an Instance with the position / rotation
  // transform from the editable scene graph.
  return make_named<render::Disk>(Vector3d::null, Vector3d(0, 1, 0), m_radius);
}

bool Disk::supportsPlanarSceneMarker() const {
  return true;
}

static bool dummy = ElementFactory::self().registerClass<Disk>("Disk");
