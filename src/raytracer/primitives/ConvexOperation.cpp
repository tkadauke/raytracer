#include "raytracer/State.h"
#include "raytracer/primitives/ConvexOperation.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

const Primitive* ConvexOperation::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  if (!boundingBoxIntersects(ray)) {
    state.miss("ConvexOperation, bounding box miss");
    return nullptr;
  }
  
  // unlike the intersection methods for box, sphere, etc, convexIntersect()
  // only returns a single (closest) hitpoint. So we need to shoot a ray in the
  // opposite direction as well.
  
  // calculate hitpoint in ray direction
  if (convexIntersect(ray, hitPoints)) {
    // if it's a hit, do it again in the opposite direction
    HitPointInterval opposite;
    // If we double the hit distance and take the longest possible length in
    // the bounding box, we should be beyond the other side of the object
    Rayd oppositeRay(
      ray.at(hitPoints.min().distance() * 2.0 + boundingBox().size().length()),
      -ray.direction()
    );
    
    // fire!
    convexIntersect(oppositeRay, opposite);
    HitPoint oppositePoint = opposite.min();
    // now, we have to project that point on the opposite side back to the
    // original ray to find the real distance value
    oppositePoint.setDistance(ray.projectedDistance(oppositePoint.point()));
    
    // add it to the interval
    hitPoints.add(oppositePoint);
    // and merge both points into a single interval. we can do that, since this
    // is by definition a convex object, so there can be only a single interval
    hitPoints = hitPoints.merged();
    
    auto hitPoint = hitPoints.minWithPositiveDistance();
    if (hitPoint.isUndefined()) {
      return nullptr;
      state.miss("ConvexOperation, ray miss");
    } else {
      state.hit("ConvexOperation");

      hitPoints.setPrimitive(this);
      return this;
    }
  }
  state.miss("ConvexOperation, ray miss");
  return nullptr;
}

bool ConvexOperation::intersects(const Rayd& ray, State&) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }
  
  HitPointInterval hitPoints;
  return convexIntersect(ray, hitPoints);
}
