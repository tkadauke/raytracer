#include "render/IntersectionSceneCompiler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "core/math/Quadric.h"
#include "render/primitives/Scene.h"

using namespace render;

namespace {
  constexpr std::uint32_t leafNodeFlag = 1;
  constexpr std::uint32_t maxBvhLeafPrimitiveCount = 4;

  bool isIdentityTransform(const Primitive::TransformedLeaf& leaf) {
    return leaf.pointMatrix == Matrix4d() && leaf.normalMatrix == Matrix3d();
  }

  class FlatIntersectionBvhBuilder {
  public:
    void build(std::vector<FlatIntersectionBvhNode>& bvh,
               std::vector<IntersectionPrimitiveRecord>& primitives) const {
      if (!primitives.empty()) {
        buildNode(bvh, primitives, 0, static_cast<std::uint32_t>(primitives.size()));
      }
    }

  private:
    std::uint32_t buildNode(std::vector<FlatIntersectionBvhNode>& bvh,
                            std::vector<IntersectionPrimitiveRecord>& primitives,
                            std::uint32_t first, std::uint32_t count) const {
      const std::uint32_t nodeIndex = static_cast<std::uint32_t>(bvh.size());
      bvh.push_back({});

      const BoundingBoxd bounds = primitiveRangeBounds(primitives, first, count);
      if (count <= maxBvhLeafPrimitiveCount) {
        bvh[nodeIndex] = FlatIntersectionBvhNode::leaf(bounds, first, count);
        return nodeIndex;
      }

      const std::uint32_t leftCount = chooseSahSplit(primitives, first, count, bounds);
      const std::uint32_t rightCount = count - leftCount;
      const std::uint32_t leftChild = buildNode(bvh, primitives, first, leftCount);
      const std::uint32_t rightChild = buildNode(bvh, primitives, first + leftCount, rightCount);
      bvh[nodeIndex] = FlatIntersectionBvhNode::branch(bounds, leftChild, rightChild);
      return nodeIndex;
    }

    std::uint32_t chooseSahSplit(std::vector<IntersectionPrimitiveRecord>& primitives,
                                 std::uint32_t first, std::uint32_t count,
                                 const BoundingBoxd& bounds) const {
      const int axis = longestCentroidAxis(primitives, first, count);
      const auto begin = primitives.begin() + first;
      const auto end = begin + count;
      std::stable_sort(begin, end,
                       [this, axis](const IntersectionPrimitiveRecord& lhs,
                                    const IntersectionPrimitiveRecord& rhs) {
                         return finiteCentroidComponent(lhs, axis) <
                                finiteCentroidComponent(rhs, axis);
                       });

      const std::uint32_t medianSplit = count / 2;
      const std::optional<std::uint32_t> sahSplit = bestSahSplit(primitives, first, count, bounds);
      if (!sahSplit) {
        return medianSplit;
      }
      return *sahSplit;
    }

    std::optional<std::uint32_t>
    bestSahSplit(const std::vector<IntersectionPrimitiveRecord>& primitives, std::uint32_t first,
                 std::uint32_t count, const BoundingBoxd& bounds) const {
      std::vector<BoundingBoxd> rightBounds(count);
      rightBounds[count - 1] = primitives[first + count - 1].bounds;
      for (std::uint32_t offset = count - 1; offset-- > 0;) {
        rightBounds[offset] = rightBounds[offset + 1];
        rightBounds[offset].include(primitives[first + offset].bounds);
      }

      const double leafCost = surfaceArea(bounds) * count;
      double bestCost = std::numeric_limits<double>::infinity();
      std::uint32_t bestSplit = 0;
      BoundingBoxd leftBounds;
      for (std::uint32_t offset = 0; offset != count - 1; ++offset) {
        leftBounds.include(primitives[first + offset].bounds);
        const std::uint32_t leftCount = offset + 1;
        const std::uint32_t rightCount = count - leftCount;
        const double cost =
          surfaceArea(leftBounds) * leftCount + surfaceArea(rightBounds[offset + 1]) * rightCount;
        if (cost < bestCost) {
          bestCost = cost;
          bestSplit = leftCount;
        }
      }

      if (!std::isfinite(bestCost) || bestCost >= leafCost) {
        return std::nullopt;
      }
      return bestSplit;
    }

    BoundingBoxd primitiveRangeBounds(const std::vector<IntersectionPrimitiveRecord>& primitives,
                                      std::uint32_t first, std::uint32_t count) const {
      BoundingBoxd bounds;
      for (std::uint32_t index = first; index != first + count; ++index) {
        bounds.include(primitives[index].bounds);
      }
      return bounds;
    }

    int longestCentroidAxis(const std::vector<IntersectionPrimitiveRecord>& primitives,
                            std::uint32_t first, std::uint32_t count) const {
      BoundingBoxd bounds;
      for (std::uint32_t index = first; index != first + count; ++index) {
        bounds.include(centroid(primitives[index]));
      }

      const Vector3d size = bounds.size();
      int axis = 0;
      if (size[1] > size[axis])
        axis = 1;
      if (size[2] > size[axis])
        axis = 2;
      return axis;
    }

    Vector3d centroid(const IntersectionPrimitiveRecord& primitive) const {
      const Vector3d value = primitive.bounds.center();
      return Vector3d(finiteComponent(value.x()), finiteComponent(value.y()),
                      finiteComponent(value.z()));
    }

    double finiteCentroidComponent(const IntersectionPrimitiveRecord& primitive, int axis) const {
      return centroid(primitive)[axis];
    }

    double finiteComponent(double value) const {
      return std::isfinite(value) ? value : 0.0;
    }

    double surfaceArea(const BoundingBoxd& bounds) const {
      if (!bounds.isValid())
        return 0.0;
      const Vector3d size = bounds.size();
      return 2.0 * (size.x() * size.y() + size.y() * size.z() + size.z() * size.x());
    }
  };
}

FlatIntersectionBvhNode FlatIntersectionBvhNode::leaf(const BoundingBoxd& bounds,
                                                      std::uint32_t firstPrimitive,
                                                      std::uint32_t primitiveCount) {
  return FlatIntersectionBvhNode{bounds, firstPrimitive, primitiveCount, leafNodeFlag};
}

FlatIntersectionBvhNode FlatIntersectionBvhNode::branch(const BoundingBoxd& bounds,
                                                        std::uint32_t leftChild,
                                                        std::uint32_t rightChild) {
  return FlatIntersectionBvhNode{bounds, leftChild, rightChild, 0};
}

bool FlatIntersectionBvhNode::isLeaf() const {
  return (flags & leafNodeFlag) != 0;
}

Vector3d IntersectionOpenCylinderPayload::normalAt(const Vector3d& point) const {
  if (radius == 0.0) {
    return Vector3d::null;
  }
  return Vector3d(point.x() / radius, 0.0, point.z() / radius);
}

Vector2d IntersectionOpenCylinderPayload::sideUvAt(const Vector3d& point) const {
  constexpr double twoPi = 6.28318530717958647692;
  double u = std::atan2(point.z(), point.x()) / twoPi;
  if (u < 0.0) {
    u += 1.0;
  }

  const double height = 2.0 * halfHeight;
  const double v = height == 0.0 ? 0.0 : (point.y() + halfHeight) / height;
  return Vector2d(u, v);
}

std::uint32_t FlatIntersectionBvhNode::firstPrimitive() const {
  return leftOrFirstPrimitive;
}

std::uint32_t FlatIntersectionBvhNode::leafPrimitiveCount() const {
  return isLeaf() ? primitiveCount : 0;
}

std::uint32_t FlatIntersectionBvhNode::leftChild() const {
  return isLeaf() ? 0 : leftOrFirstPrimitive;
}

std::uint32_t FlatIntersectionBvhNode::rightChild() const {
  return isLeaf() ? 0 : primitiveCount;
}

bool CompiledIntersectionScene::fullySupported() const {
  return m_unsupportedPrimitives.empty();
}

BoundingBoxd CompiledIntersectionScene::bounds() const {
  if (m_bvh.empty())
    return BoundingBoxd::undefined;
  return m_bvh.front().bounds;
}

const std::vector<FlatIntersectionBvhNode>& CompiledIntersectionScene::bvh() const {
  return m_bvh;
}

const std::vector<IntersectionPrimitiveRecord>& CompiledIntersectionScene::primitives() const {
  return m_primitives;
}

const std::vector<IntersectionTrianglePayload>& CompiledIntersectionScene::triangles() const {
  return m_triangles;
}

const std::vector<IntersectionSpherePayload>& CompiledIntersectionScene::spheres() const {
  return m_spheres;
}

const std::vector<IntersectionPlanePayload>& CompiledIntersectionScene::planes() const {
  return m_planes;
}

const std::vector<IntersectionRectanglePayload>& CompiledIntersectionScene::rectangles() const {
  return m_rectangles;
}

const std::vector<IntersectionDiskPayload>& CompiledIntersectionScene::disks() const {
  return m_disks;
}

const std::vector<IntersectionOpenCylinderPayload>&
CompiledIntersectionScene::openCylinders() const {
  return m_openCylinders;
}

const std::vector<IntersectionTransformPayload>& CompiledIntersectionScene::transforms() const {
  return m_transforms;
}

const std::vector<std::shared_ptr<Material>>& CompiledIntersectionScene::materials() const {
  return m_materials;
}

const std::vector<const Primitive*>& CompiledIntersectionScene::objects() const {
  return m_objects;
}

const std::vector<UnsupportedIntersectionPrimitive>&
CompiledIntersectionScene::unsupportedPrimitives() const {
  return m_unsupportedPrimitives;
}

void IntersectionSceneBuilder::addUnsupportedPrimitive(const Primitive::TransformedLeaf& leaf,
                                                       std::string reason) {
  const IntersectionObjectId object = objectIdFor(leaf.primitive);
  addPrimitive(leaf, IntersectionPrimitiveKind::Unsupported, 0, 0);
  m_scene.m_unsupportedPrimitives.push_back(
    UnsupportedIntersectionPrimitive{object, leaf.primitive->name(), std::move(reason)});
}

void IntersectionSceneBuilder::addTriangle(const Primitive::TransformedLeaf& leaf,
                                           const IntersectionTrianglePayload& payload) {
  addTriangle(leaf, payload, leaf.boundingBox());
}

void IntersectionSceneBuilder::addTriangle(const Primitive::TransformedLeaf& leaf,
                                           const IntersectionTrianglePayload& payload,
                                           const BoundingBoxd& bounds) {
  const std::uint32_t offset = static_cast<std::uint32_t>(m_scene.m_triangles.size());
  m_scene.m_triangles.push_back(payload);
  addPrimitive(leaf, IntersectionPrimitiveKind::Triangle, bounds, offset, 1);
}

void IntersectionSceneBuilder::addSphere(const Primitive::TransformedLeaf& leaf,
                                         const Vector3d& center, double radius) {
  const std::uint32_t offset = static_cast<std::uint32_t>(m_scene.m_spheres.size());
  m_scene.m_spheres.push_back(IntersectionSpherePayload{center, radius});
  addPrimitive(leaf, IntersectionPrimitiveKind::Sphere, offset, 1);
}

void IntersectionSceneBuilder::addPlane(const Primitive::TransformedLeaf& leaf,
                                        const Vector3d& normal, double distance) {
  const std::uint32_t offset = static_cast<std::uint32_t>(m_scene.m_planes.size());
  m_scene.m_planes.push_back(IntersectionPlanePayload{normal, distance});
  addPrimitive(leaf, IntersectionPrimitiveKind::Plane, offset, 1);
}

void IntersectionSceneBuilder::addRectangle(const Primitive::TransformedLeaf& leaf,
                                            const Vector3d& corner, const Vector3d& leg1,
                                            const Vector3d& leg2, const Vector3d& normal) {
  const std::uint32_t offset = static_cast<std::uint32_t>(m_scene.m_rectangles.size());
  m_scene.m_rectangles.push_back(IntersectionRectanglePayload{corner, leg1, leg2, normal});
  addPrimitive(leaf, IntersectionPrimitiveKind::Rectangle, offset, 1);
}

void IntersectionSceneBuilder::addDisk(const Primitive::TransformedLeaf& leaf,
                                       const Vector3d& center, const Vector3d& normal,
                                       double radius) {
  const std::uint32_t offset = static_cast<std::uint32_t>(m_scene.m_disks.size());
  m_scene.m_disks.push_back(IntersectionDiskPayload{center, normal, radius});
  addPrimitive(leaf, IntersectionPrimitiveKind::Disk, offset, 1);
}

void IntersectionSceneBuilder::addOpenCylinder(const Primitive::TransformedLeaf& leaf,
                                               double radius, double halfHeight) {
  const std::uint32_t offset = static_cast<std::uint32_t>(m_scene.m_openCylinders.size());
  m_scene.m_openCylinders.push_back(IntersectionOpenCylinderPayload{radius, halfHeight});
  addPrimitive(leaf, IntersectionPrimitiveKind::OpenCylinder, offset, 1);
}

CompiledIntersectionScene IntersectionSceneBuilder::finish() {
  FlatIntersectionBvhBuilder().build(m_scene.m_bvh, m_scene.m_primitives);
  return std::move(m_scene);
}

IntersectionMaterialId IntersectionSceneBuilder::materialIdFor(std::shared_ptr<Material> material) {
  if (!material)
    return 0;

  if (m_scene.m_materials.empty())
    m_scene.m_materials.push_back(nullptr);

  const auto existing = m_materialIds.find(material.get());
  if (existing != m_materialIds.end())
    return existing->second;

  const IntersectionMaterialId id = static_cast<IntersectionMaterialId>(m_scene.m_materials.size());
  m_materialIds.emplace(material.get(), id);
  m_scene.m_materials.push_back(std::move(material));
  return id;
}

IntersectionObjectId IntersectionSceneBuilder::objectIdFor(const Primitive* primitive) {
  if (!primitive)
    return 0;

  if (m_scene.m_objects.empty())
    m_scene.m_objects.push_back(nullptr);

  const auto existing = m_objectIds.find(primitive);
  if (existing != m_objectIds.end())
    return existing->second;

  const IntersectionObjectId id = static_cast<IntersectionObjectId>(m_scene.m_objects.size());
  m_objectIds.emplace(primitive, id);
  m_scene.m_objects.push_back(primitive);
  return id;
}

IntersectionTransformId
IntersectionSceneBuilder::transformIdFor(const Primitive::TransformedLeaf& leaf) {
  if (isIdentityTransform(leaf))
    return 0;

  if (m_scene.m_transforms.empty())
    m_scene.m_transforms.push_back(
      IntersectionTransformPayload{Matrix4d(), Matrix3d(), Matrix4d(), Matrix3d()});

  for (std::size_t index = 1; index != m_scene.m_transforms.size(); ++index) {
    const IntersectionTransformPayload& transform = m_scene.m_transforms[index];
    if (transform.pointMatrix == leaf.pointMatrix && transform.normalMatrix == leaf.normalMatrix)
      return static_cast<IntersectionTransformId>(index);
  }

  const IntersectionTransformId id =
    static_cast<IntersectionTransformId>(m_scene.m_transforms.size());
  const Matrix4d inversePointMatrix = leaf.pointMatrix.inverted();
  m_scene.m_transforms.push_back(IntersectionTransformPayload{
    leaf.pointMatrix, leaf.normalMatrix, inversePointMatrix, Matrix3d(inversePointMatrix)});
  return id;
}

void IntersectionSceneBuilder::addPrimitive(const Primitive::TransformedLeaf& leaf,
                                            IntersectionPrimitiveKind kind,
                                            std::uint32_t payloadOffset,
                                            std::uint32_t payloadCount) {
  addPrimitive(leaf, kind, leaf.boundingBox(), payloadOffset, payloadCount);
}

void IntersectionSceneBuilder::addPrimitive(const Primitive::TransformedLeaf& leaf,
                                            IntersectionPrimitiveKind kind,
                                            const BoundingBoxd& bounds, std::uint32_t payloadOffset,
                                            std::uint32_t payloadCount) {
  m_scene.m_primitives.push_back(
    IntersectionPrimitiveRecord{kind, materialIdFor(leaf.material), objectIdFor(leaf.primitive),
                                transformIdFor(leaf), bounds, payloadOffset, payloadCount});
}

CompiledIntersectionScene IntersectionSceneCompiler::compile(const Scene& scene) const {
  IntersectionSceneBuilder builder;
  scene.appendIntersectionSceneRecords(builder, nullptr, Matrix4d(), Matrix3d());
  return builder.finish();
}

CompiledIntersectionHit
CompiledIntersectionSceneIntersector::intersectClosest(const CompiledIntersectionScene& scene,
                                                       const Rayd& ray) const {
  CompiledIntersectionHit closest;
  double closestDistance = std::numeric_limits<double>::infinity();

  if (scene.bvh().empty()) {
    return closest;
  }

  std::vector<std::uint32_t> stack;
  stack.push_back(0);
  while (!stack.empty()) {
    const std::uint32_t nodeIndex = stack.back();
    stack.pop_back();
    if (nodeIndex >= scene.bvh().size()) {
      continue;
    }

    const FlatIntersectionBvhNode& node = scene.bvh()[nodeIndex];
    if (!boundsIntersectRay(node.bounds, ray, closestDistance)) {
      continue;
    }

    if (!node.isLeaf()) {
      pushIntersectingChildren(scene, node, ray, closestDistance, stack);
      continue;
    }

    for (std::uint32_t offset = 0; offset != node.leafPrimitiveCount(); ++offset) {
      const std::uint32_t primitiveIndex = node.firstPrimitive() + offset;
      if (primitiveIndex >= scene.primitives().size()) {
        continue;
      }

      const IntersectionPrimitiveRecord& primitive = scene.primitives()[primitiveIndex];
      if (!boundsIntersectRay(primitive.bounds, ray, closestDistance)) {
        continue;
      }

      const std::optional<CompiledIntersectionHit> hit = intersectPrimitive(scene, primitive, ray);
      if (hit && hit->distance < closestDistance) {
        closest = *hit;
        closest.primitiveRecord = primitiveIndex;
        closestDistance = hit->distance;
      }
    }
  }

  return closest;
}

bool CompiledIntersectionSceneIntersector::intersectAny(const CompiledIntersectionScene& scene,
                                                        const Rayd& ray, double maxDistance) const {
  if (maxDistance <= 0.0) {
    return false;
  }

  if (scene.bvh().empty()) {
    return false;
  }

  std::vector<std::uint32_t> stack;
  stack.push_back(0);
  while (!stack.empty()) {
    const std::uint32_t nodeIndex = stack.back();
    stack.pop_back();
    if (nodeIndex >= scene.bvh().size()) {
      continue;
    }

    const FlatIntersectionBvhNode& node = scene.bvh()[nodeIndex];
    if (!boundsIntersectRay(node.bounds, ray, maxDistance)) {
      continue;
    }

    if (!node.isLeaf()) {
      pushIntersectingChildren(scene, node, ray, maxDistance, stack);
      continue;
    }

    for (std::uint32_t offset = 0; offset != node.leafPrimitiveCount(); ++offset) {
      const std::uint32_t primitiveIndex = node.firstPrimitive() + offset;
      if (primitiveIndex >= scene.primitives().size()) {
        continue;
      }

      const IntersectionPrimitiveRecord& primitive = scene.primitives()[primitiveIndex];
      if (!boundsIntersectRay(primitive.bounds, ray, maxDistance)) {
        continue;
      }

      const std::optional<CompiledIntersectionHit> hit = intersectPrimitive(scene, primitive, ray);
      if (hit && hitOccludes(*hit, maxDistance)) {
        return true;
      }
    }
  }

  return false;
}

std::optional<CompiledIntersectionHit> CompiledIntersectionSceneIntersector::intersectPrimitive(
  const CompiledIntersectionScene& scene, const IntersectionPrimitiveRecord& primitive,
  const Rayd& ray) const {
  if (primitive.payloadCount != 1) {
    return std::nullopt;
  }

  switch (primitive.kind) {
  case IntersectionPrimitiveKind::Triangle:
    return intersectTriangle(scene, primitive, ray);
  case IntersectionPrimitiveKind::Sphere:
    return intersectSphere(scene, primitive, ray);
  case IntersectionPrimitiveKind::Plane:
    return intersectPlane(scene, primitive, ray);
  case IntersectionPrimitiveKind::Rectangle:
    return intersectRectangle(scene, primitive, ray);
  case IntersectionPrimitiveKind::Disk:
    return intersectDisk(scene, primitive, ray);
  case IntersectionPrimitiveKind::OpenCylinder:
    return intersectOpenCylinder(scene, primitive, ray);
  case IntersectionPrimitiveKind::Unsupported:
    return std::nullopt;
  }

  return std::nullopt;
}

bool CompiledIntersectionSceneIntersector::boundsIntersectRay(const BoundingBoxd& bounds,
                                                              const Rayd& ray,
                                                              double maxDistance) const {
  return boundsRayEntryDistance(bounds, ray, maxDistance).has_value();
}

std::optional<double> CompiledIntersectionSceneIntersector::boundsRayEntryDistance(
  const BoundingBoxd& bounds, const Rayd& ray, double maxDistance) const {
  Ranged interval(0.0, 0.0);
  if (!bounds.intersect(ray, interval)) {
    return std::nullopt;
  }

  if (interval.begin() > maxDistance) {
    return std::nullopt;
  }

  return std::max(0.0, interval.begin());
}

void CompiledIntersectionSceneIntersector::pushIntersectingChildren(
  const CompiledIntersectionScene& scene, const FlatIntersectionBvhNode& node, const Rayd& ray,
  double maxDistance, std::vector<std::uint32_t>& stack) const {
  const std::uint32_t leftChild = node.leftChild();
  const std::uint32_t rightChild = node.rightChild();
  const std::optional<double> leftEntry =
    leftChild < scene.bvh().size()
      ? boundsRayEntryDistance(scene.bvh()[leftChild].bounds, ray, maxDistance)
      : std::nullopt;
  const std::optional<double> rightEntry =
    rightChild < scene.bvh().size()
      ? boundsRayEntryDistance(scene.bvh()[rightChild].bounds, ray, maxDistance)
      : std::nullopt;

  if (leftEntry && rightEntry) {
    if (*leftEntry <= *rightEntry) {
      stack.push_back(rightChild);
      stack.push_back(leftChild);
    } else {
      stack.push_back(leftChild);
      stack.push_back(rightChild);
    }
  } else if (leftEntry) {
    stack.push_back(leftChild);
  } else if (rightEntry) {
    stack.push_back(rightChild);
  }
}

bool CompiledIntersectionSceneIntersector::hitOccludes(const CompiledIntersectionHit& hit,
                                                       double maxDistance) const {
  if (!hit.hit) {
    return false;
  }

  if (std::isinf(maxDistance)) {
    return true;
  }

  const double occlusionLimit = std::max(0.0, maxDistance - Rayd::epsilon * 4.0);
  return hit.distance < occlusionLimit;
}

std::optional<CompiledIntersectionSceneIntersector::PrimitiveSpaceRay>
CompiledIntersectionSceneIntersector::rayForPrimitive(const CompiledIntersectionScene& scene,
                                                      const IntersectionPrimitiveRecord& primitive,
                                                      const Rayd& ray) const {
  if (primitive.transform == 0) {
    return PrimitiveSpaceRay{ray, nullptr};
  }

  if (primitive.transform >= scene.transforms().size()) {
    return std::nullopt;
  }

  const IntersectionTransformPayload& transform = scene.transforms()[primitive.transform];
  return PrimitiveSpaceRay{
    Rayd(Vector4d(transform.inversePointMatrix.transformPoint(Vector3d(ray.origin()))),
         transform.inverseDirectionMatrix * ray.direction()),
    &transform};
}

Vector4d
CompiledIntersectionSceneIntersector::hitPointForPrimitive(const PrimitiveSpaceRay& primitiveRay,
                                                           const Vector4d& point) const {
  if (!primitiveRay.transform) {
    return point;
  }

  return primitiveRay.transform->pointMatrix * point;
}

Vector3d
CompiledIntersectionSceneIntersector::hitNormalForPrimitive(const PrimitiveSpaceRay& primitiveRay,
                                                            const Vector3d& normal) const {
  if (!primitiveRay.transform) {
    return normal;
  }

  return (primitiveRay.transform->normalMatrix * normal).normalized();
}

CompiledIntersectionHit CompiledIntersectionSceneIntersector::makeHit(
  const IntersectionPrimitiveRecord& primitive, const PrimitiveSpaceRay& primitiveRay,
  double distance, const Vector4d& localPoint, const Vector3d& localNormal, const Vector2d& uv,
  const Vector3d& barycentric) const {
  return CompiledIntersectionHit{
    true,
    primitive.material,
    primitive.object,
    0,
    distance,
    hitPointForPrimitive(primitiveRay, localPoint),
    hitNormalForPrimitive(primitiveRay, localNormal),
    uv,
    barycentric,
  };
}

std::optional<CompiledIntersectionHit> CompiledIntersectionSceneIntersector::intersectTriangle(
  const CompiledIntersectionScene& scene, const IntersectionPrimitiveRecord& primitive,
  const Rayd& ray) const {
  if (primitive.payloadOffset >= scene.triangles().size()) {
    return std::nullopt;
  }

  const std::optional<PrimitiveSpaceRay> primitiveRay = rayForPrimitive(scene, primitive, ray);
  if (!primitiveRay) {
    return std::nullopt;
  }

  const IntersectionTrianglePayload& payload = scene.triangles()[primitive.payloadOffset];
  const Rayd& localRay = primitiveRay->ray;
  const Vector3d& point0 = payload.point0;
  const Vector3d& point1 = payload.point1;
  const Vector3d& point2 = payload.point2;

  const double a = point0.x() - point1.x();
  const double b = point0.x() - point2.x();
  const double c = localRay.direction().x();
  const double d = point0.x() - localRay.origin().x();
  const double e = point0.y() - point1.y();
  const double f = point0.y() - point2.y();
  const double g = localRay.direction().y();
  const double h = point0.y() - localRay.origin().y();
  const double i = point0.z() - point1.z();
  const double j = point0.z() - point2.z();
  const double k = localRay.direction().z();
  const double l = point0.z() - localRay.origin().z();

  const double m = f * k - g * j;
  const double n = h * k - g * l;
  const double p = f * l - h * j;
  const double q = g * i - e * k;
  const double r = e * l - h * i;
  const double s = e * j - f * i;

  const double denominator = a * m + b * q + c * s;
  if (denominator == 0.0) {
    return std::nullopt;
  }

  const double invDenom = 1.0 / denominator;
  const double beta = (d * m - b * n - c * p) * invDenom;
  if (beta < 0.0 || beta > 1.0) {
    return std::nullopt;
  }

  const double gamma = (a * n + d * q + c * r) * invDenom;
  if (gamma < 0.0 || gamma > 1.0 || beta + gamma > 1.0) {
    return std::nullopt;
  }

  const double distance = (a * p - b * r + d * s) * invDenom;
  if (distance < payload.minimumHitDistance) {
    return std::nullopt;
  }

  const double alpha = 1.0 - beta - gamma;
  const Vector3d barycentric(alpha, beta, gamma);
  const Vector3d normal =
    (payload.normal0 * alpha + payload.normal1 * beta + payload.normal2 * gamma).normalized();
  const Vector2d uv = payload.uv0 * alpha + payload.uv1 * beta + payload.uv2 * gamma;

  return makeHit(primitive, *primitiveRay, distance, localRay.at(distance), normal, uv,
                 barycentric);
}

std::optional<CompiledIntersectionHit>
CompiledIntersectionSceneIntersector::intersectSphere(const CompiledIntersectionScene& scene,
                                                      const IntersectionPrimitiveRecord& primitive,
                                                      const Rayd& ray) const {
  if (primitive.payloadOffset >= scene.spheres().size()) {
    return std::nullopt;
  }

  const std::optional<PrimitiveSpaceRay> primitiveRay = rayForPrimitive(scene, primitive, ray);
  if (!primitiveRay) {
    return std::nullopt;
  }

  const IntersectionSpherePayload& payload = scene.spheres()[primitive.payloadOffset];
  const Rayd& localRay = primitiveRay->ray;
  const Vector3d origin = Vector3d(localRay.origin()) - payload.center;
  const Vector3d& direction = localRay.direction();
  const double od = origin * direction;
  const double dd = direction * direction;
  const double discriminant = od * od - dd * (origin * origin - payload.radius * payload.radius);
  if (discriminant <= 0.0) {
    return std::nullopt;
  }

  const double discriminantRoot = std::sqrt(discriminant);
  const double nearDistance = (-od - discriminantRoot) / dd;
  const double farDistance = (-od + discriminantRoot) / dd;
  if (nearDistance <= 0.0 && farDistance <= 0.0) {
    return std::nullopt;
  }

  const double distance = nearDistance > 0.0 ? nearDistance : farDistance;
  const Vector4d point = localRay.at(distance);
  const Vector3d normal = (Vector3d(point) - payload.center) / payload.radius;
  return makeHit(primitive, *primitiveRay, distance, point, normal);
}

std::optional<CompiledIntersectionHit>
CompiledIntersectionSceneIntersector::intersectPlane(const CompiledIntersectionScene& scene,
                                                     const IntersectionPrimitiveRecord& primitive,
                                                     const Rayd& ray) const {
  if (primitive.payloadOffset >= scene.planes().size()) {
    return std::nullopt;
  }

  const std::optional<PrimitiveSpaceRay> primitiveRay = rayForPrimitive(scene, primitive, ray);
  if (!primitiveRay) {
    return std::nullopt;
  }

  const IntersectionPlanePayload& payload = scene.planes()[primitive.payloadOffset];
  const Rayd& localRay = primitiveRay->ray;
  const double angle = payload.normal * localRay.direction();
  if (angle == 0.0) {
    return std::nullopt;
  }

  const double distance =
    -(payload.normal * Vector3d(localRay.origin()) + payload.distance) / angle;
  if (distance <= 0.0) {
    return std::nullopt;
  }

  return makeHit(primitive, *primitiveRay, distance, localRay.at(distance), payload.normal);
}

std::optional<CompiledIntersectionHit> CompiledIntersectionSceneIntersector::intersectRectangle(
  const CompiledIntersectionScene& scene, const IntersectionPrimitiveRecord& primitive,
  const Rayd& ray) const {
  if (primitive.payloadOffset >= scene.rectangles().size()) {
    return std::nullopt;
  }

  const std::optional<PrimitiveSpaceRay> primitiveRay = rayForPrimitive(scene, primitive, ray);
  if (!primitiveRay) {
    return std::nullopt;
  }

  const IntersectionRectanglePayload& payload = scene.rectangles()[primitive.payloadOffset];
  const Rayd& localRay = primitiveRay->ray;
  const double denominator = localRay.direction() * payload.normal;
  if (denominator == 0.0) {
    return std::nullopt;
  }

  const double distance =
    (payload.corner - Vector3d(localRay.origin())) * payload.normal / denominator;
  if (!std::isfinite(distance)) {
    return std::nullopt;
  }

  const Vector4d point = localRay.at(distance);
  const Vector3d difference = Vector3d(point) - payload.corner;
  const double squaredLength1 = payload.leg1.squaredLength();
  const double squaredLength2 = payload.leg2.squaredLength();
  const double dot1 = difference * payload.leg1;
  if (dot1 < 0.0 || dot1 > squaredLength1) {
    return std::nullopt;
  }

  const double dot2 = difference * payload.leg2;
  if (dot2 < 0.0 || dot2 > squaredLength2) {
    return std::nullopt;
  }

  if (distance < 0.0) {
    return std::nullopt;
  }

  return makeHit(primitive, *primitiveRay, distance, point, payload.normal);
}

std::optional<CompiledIntersectionHit>
CompiledIntersectionSceneIntersector::intersectDisk(const CompiledIntersectionScene& scene,
                                                    const IntersectionPrimitiveRecord& primitive,
                                                    const Rayd& ray) const {
  if (primitive.payloadOffset >= scene.disks().size()) {
    return std::nullopt;
  }

  const std::optional<PrimitiveSpaceRay> primitiveRay = rayForPrimitive(scene, primitive, ray);
  if (!primitiveRay) {
    return std::nullopt;
  }

  const IntersectionDiskPayload& payload = scene.disks()[primitive.payloadOffset];
  const Rayd& localRay = primitiveRay->ray;
  const double denominator = localRay.direction() * payload.normal;
  if (denominator == 0.0) {
    return std::nullopt;
  }

  const double distance =
    (payload.center - Vector3d(localRay.origin())) * payload.normal / denominator;
  if (!std::isfinite(distance)) {
    return std::nullopt;
  }

  const Vector4d point = localRay.at(distance);

  if (!(Vector3d(point).squaredDistanceTo(payload.center) < payload.radius * payload.radius)) {
    return std::nullopt;
  }

  if (distance < 0.0001) {
    return std::nullopt;
  }

  return makeHit(primitive, *primitiveRay, distance, point, payload.normal);
}

std::optional<CompiledIntersectionHit> CompiledIntersectionSceneIntersector::intersectOpenCylinder(
  const CompiledIntersectionScene& scene, const IntersectionPrimitiveRecord& primitive,
  const Rayd& ray) const {
  if (primitive.payloadOffset >= scene.openCylinders().size()) {
    return std::nullopt;
  }

  const std::optional<PrimitiveSpaceRay> primitiveRay = rayForPrimitive(scene, primitive, ray);
  if (!primitiveRay) {
    return std::nullopt;
  }

  const IntersectionOpenCylinderPayload& payload = scene.openCylinders()[primitive.payloadOffset];
  const Rayd& localRay = primitiveRay->ray;
  const Vector3d origin(localRay.origin());
  const Vector3d& direction = localRay.direction();

  double roots[2] = {};
  const double a = direction.x() * direction.x() + direction.z() * direction.z();
  const double b = 2.0 * (origin.x() * direction.x() + origin.z() * direction.z());
  const double c =
    origin.x() * origin.x() + origin.z() * origin.z() - payload.radius * payload.radius;
  const int rootCount = Quadric<double>(a, b, c).solveInto(roots);
  if (rootCount < 2) {
    return std::nullopt;
  }

  double bestDistance = std::numeric_limits<double>::infinity();
  Vector4d bestPoint;
  for (double distance : roots) {
    if (distance <= 0.0 || distance >= bestDistance) {
      continue;
    }

    const Vector4d point = localRay.at(distance);
    if (point.y() < -payload.halfHeight || point.y() > payload.halfHeight) {
      continue;
    }

    bestDistance = distance;
    bestPoint = point;
  }

  if (!std::isfinite(bestDistance)) {
    return std::nullopt;
  }

  const Vector3d point(bestPoint);
  return makeHit(primitive, *primitiveRay, bestDistance, bestPoint, payload.normalAt(point),
                 payload.sideUvAt(point));
}

const char* render::toString(IntersectionPrimitiveKind kind) {
  switch (kind) {
  case IntersectionPrimitiveKind::Unsupported:
    return "unsupported";
  case IntersectionPrimitiveKind::Triangle:
    return "triangle";
  case IntersectionPrimitiveKind::Sphere:
    return "sphere";
  case IntersectionPrimitiveKind::Plane:
    return "plane";
  case IntersectionPrimitiveKind::Rectangle:
    return "rectangle";
  case IntersectionPrimitiveKind::Disk:
    return "disk";
  case IntersectionPrimitiveKind::OpenCylinder:
    return "open_cylinder";
  }

  return "unknown";
}
