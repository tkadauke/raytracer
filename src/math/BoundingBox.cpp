#include "math/BoundingBox.h"
#include "math/Ray.h"

const BoundingBox& BoundingBox::undefined() {
  static BoundingBox* b = new BoundingBox(Vector3d::undefined(), Vector3d::undefined());
  return *b;
}
