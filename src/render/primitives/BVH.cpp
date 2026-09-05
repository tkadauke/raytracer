#include "render/primitives/BVH.h"

#include "core/SimdFeatures.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "render/State.h"

#include <algorithm>
#include <limits>

using namespace render;

namespace {
  // Centroid of a primitive's AABB. Used to sort primitives along the
  // split axis — sorting by centroid (not by AABB extent) keeps
  // straddling primitives from biasing the partition toward one side.
  Vector3d centroidOf(const Primitive& p) {
    const auto& b = p.boundingBox();
    return (b.min() + b.max()) * 0.5;
  }
}

BVH::~BVH() = default;

void BVH::setup() {
  std::vector<std::shared_ptr<Primitive>> prims(primitives().begin(), primitives().end());
  m_root = build(std::move(prims));
}

std::unique_ptr<BVH::Node> BVH::build(std::vector<std::shared_ptr<Primitive>> prims) const {
  auto node = std::make_unique<Node>();

  // Empty input: empty leaf with default-constructed (invalid) bbox.
  // Callers should never produce this but the recursion has to be
  // robust against it.
  if (prims.empty()) {
    return node;
  }

  // Compute the AABB tight around every input primitive's AABB.
  for (const auto& p : prims) {
    node->bbox.include(p->boundingBox());
  }

  // Leaf condition: small enough to not bother splitting.
  if (static_cast<int>(prims.size()) <= m_leafSize) {
    node->primitives = std::move(prims);
    return node;
  }

  // Split-axis selection: longest dimension of the *centroid* bounding
  // box. Centroids — not AABB extents — because that's what we sort by
  // and partition on; using AABB extents here would make the choice
  // unstable when one large primitive spans the whole scene.
  BoundingBoxd centroidBox;
  for (const auto& p : prims) {
    centroidBox.include(centroidOf(*p));
  }
  const int axis = centroidBox.longestAxis();

  // Sort by centroid along the chosen axis. The sweep below relies on
  // the primitives being ordered so that a "split at index i" means
  // "everything below i goes left, everything from i goes right".
  std::sort(prims.begin(), prims.end(),
            [axis](const std::shared_ptr<Primitive>& a, const std::shared_ptr<Primitive>& b) {
              return centroidOf(*a)[axis] < centroidOf(*b)[axis];
            });

  // Surface Area Heuristic sweep: try every N-1 split position; for
  // each compute SAH cost = SA(left)·N_left + SA(right)·N_right.
  // Track the minimum. The textbook SAH also adds a constant
  // traversal-cost term and the leaf cost has an explicit ratio; this
  // implementation drops the constants because they don't change the
  // argmin and would only matter for the "is the best split worth
  // doing" check below.
  const int n = static_cast<int>(prims.size());
  std::vector<BoundingBoxd> rightAccum(n);
  rightAccum[n - 1] = prims[n - 1]->boundingBox();
  for (int i = n - 2; i >= 0; --i) {
    rightAccum[i] = rightAccum[i + 1];
    rightAccum[i].include(prims[i]->boundingBox());
  }

  double bestCost = std::numeric_limits<double>::infinity();
  int bestSplit = -1;
  BoundingBoxd leftAccum;
  for (int i = 0; i < n - 1; ++i) {
    leftAccum.include(prims[i]->boundingBox());
    const double cost =
      leftAccum.surfaceArea() * (i + 1) + rightAccum[i + 1].surfaceArea() * (n - i - 1);
    if (cost < bestCost) {
      bestCost = cost;
      bestSplit = i;
    }
  }

  // If the best split's expected cost is worse than just keeping
  // everything as a leaf, don't split. Leaf cost is approximated as
  // SA(parent)·N (every ray that hits the parent tests every prim).
  const double leafCost = node->bbox.surfaceArea() * n;
  if (bestSplit < 0 || bestCost >= leafCost) {
    node->primitives = std::move(prims);
    return node;
  }

  // Recurse. Primitives above the split go into the left child; the
  // rest go right. Indexing by `bestSplit + 1` because sort is
  // inclusive-exclusive at the split.
  std::vector<std::shared_ptr<Primitive>> leftPrims(prims.begin(), prims.begin() + bestSplit + 1);
  std::vector<std::shared_ptr<Primitive>> rightPrims(prims.begin() + bestSplit + 1, prims.end());

  node->left = build(std::move(leftPrims));
  node->right = build(std::move(rightPrims));
  return node;
}

const Primitive* BVH::intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                render::State& state) const {
  if (!m_root) {
    // Fallback: if the user forgot to call setup(), the linear-scan
    // base implementation still produces correct results.
    return Composite::intersect(ray, hitPoints, state);
  }
  const Vector3d inverseDirection(1.0 / ray.direction().x(), 1.0 / ray.direction().y(),
                                  1.0 / ray.direction().z());
  return intersectNode(m_root.get(), ray, inverseDirection, hitPoints, state);
}

bool BVH::intersects(const Rayd& ray, render::State& state) const {
  if (!m_root) {
    return Composite::intersects(ray, state);
  }
  const Vector3d inverseDirection(1.0 / ray.direction().x(), 1.0 / ray.direction().y(),
                                  1.0 / ray.direction().z());
  return intersectsNode(m_root.get(), ray, inverseDirection, state);
}

const Primitive* BVH::intersectNode(const Node* node, const Rayd& ray,
                                    const Vector3d& inverseDirection, HitPointInterval& hitPoints,
                                    render::State& state) const {
  if (!node)
    return nullptr;
  if (!node->bbox.intersects(ray, inverseDirection))
    return nullptr;
  return intersectHitNode(node, ray, inverseDirection, hitPoints, state);
}

const Primitive* BVH::intersectHitNode(const Node* node, const Rayd& ray,
                                       const Vector3d& inverseDirection,
                                       HitPointInterval& hitPoints, render::State& state) const {
  if (node->isLeaf()) {
    const Primitive* hit = nullptr;
    double minDistance = std::numeric_limits<double>::infinity();
    for (const auto& p : node->primitives) {
      HitPointInterval candidate;
      auto primitive = p->intersect(ray, candidate, state);
      if (primitive) {
        hitPoints = hitPoints + candidate;
        const double distance = candidate.minWithPositiveDistance().distance();
        if (distance < minDistance) {
          hit = primitive;
          minDistance = distance;
        }
      }
    }
    return hit;
  }

  const Node* left = node->left.get();
  const Node* right = node->right.get();
  const bool leftHitBox = left && left->bbox.intersects(ray, inverseDirection);
  const bool rightHitBox = right && right->bbox.intersects(ray, inverseDirection);

  HitPointInterval leftPoints, rightPoints;
  const Primitive* leftHit =
    leftHitBox ? intersectHitNode(left, ray, inverseDirection, leftPoints, state) : nullptr;
  const Primitive* rightHit =
    rightHitBox ? intersectHitNode(right, ray, inverseDirection, rightPoints, state) : nullptr;

  if (!leftHit) {
    hitPoints = hitPoints + rightPoints;
    return rightHit;
  }
  if (!rightHit) {
    hitPoints = hitPoints + leftPoints;
    return leftHit;
  }

  // Keep Composite's interval contract: accumulate both children.
  hitPoints = hitPoints + leftPoints + rightPoints;
  const double leftT = leftPoints.minWithPositiveDistance().distance();
  const double rightT = rightPoints.minWithPositiveDistance().distance();
  return leftT <= rightT ? leftHit : rightHit;
}

bool BVH::intersectsNode(const Node* node, const Rayd& ray, const Vector3d& inverseDirection,
                         render::State& state) const {
  if (!node)
    return false;
  if (!node->bbox.intersects(ray, inverseDirection))
    return false;
  return intersectsHitNode(node, ray, inverseDirection, state);
}

bool BVH::intersectsHitNode(const Node* node, const Rayd& ray, const Vector3d& inverseDirection,
                            render::State& state) const {
  if (node->isLeaf()) {
    for (const auto& p : node->primitives) {
      if (p->intersects(ray, state))
        return true;
    }
    return false;
  }

  const Node* left = node->left.get();
  const Node* right = node->right.get();
  const bool leftHitBox = left && left->bbox.intersects(ray, inverseDirection);
  const bool rightHitBox = right && right->bbox.intersects(ray, inverseDirection);

  return (leftHitBox && intersectsHitNode(left, ray, inverseDirection, state)) ||
         (rightHitBox && intersectsHitNode(right, ray, inverseDirection, state));
}

RayPacketIntersection4 BVH::intersectPacket(const Ray4& rays, render::State& state) const {
  if (!m_root) {
    return Primitive::intersectPacket(rays, state);
  }
  std::array<float, Ray4::lanes> tMin;
  tMin.fill(std::numeric_limits<float>::infinity());
  uint16_t hitMask = 0;
  constexpr uint16_t allActive = static_cast<uint16_t>((1u << Ray4::lanes) - 1u);
  intersectPacketNode(m_root.get(), rays, allActive, tMin, hitMask, state);
  RayPacketIntersection4 result;
  for (std::size_t i = 0; i < Ray4::lanes; ++i) {
    if (hitMask & (1u << i)) {
      result.setHit(i, tMin[i], tMin[i]);
    }
  }
  return result;
}

void BVH::intersectPacketNode(const Node* node, const Ray4& rays, uint16_t activeMask,
                              std::array<float, Ray4::lanes>& tMin, uint16_t& hitMask,
                              render::State& state) const {
  if (!node || activeMask == 0)
    return;

  // Test each active-mask lane against this node's AABB. Lanes that miss
  // are excluded from the descending mask, pruning the subtree for those
  // rays without a separate traversal.
  const uint16_t nodeMask = static_cast<uint16_t>(
    activeMask & static_cast<uint16_t>(core::simd::movemask(node->bbox.intersects4(rays))));
  if (!nodeMask)
    return;

  if (node->isLeaf()) {
    for (const auto& prim : node->primitives) {
      const RayPacketIntersection4 r = prim->intersectPacket(rays, state);
      for (std::size_t i = 0; i < Ray4::lanes; ++i) {
        if ((nodeMask & (1u << i)) && r.hit(i)) {
          // tNear may be negative when the ray origin is inside the primitive;
          // fall back to tFar (the exit point) in that case.
          const float hitT = r.tNear[i] > 0.0f ? r.tNear[i] : r.tFar[i];
          if (hitT > 0.0f && hitT < tMin[i]) {
            tMin[i] = hitT;
            hitMask |= static_cast<uint16_t>(1u << i);
          }
        }
      }
    }
    return;
  }

  intersectPacketNode(node->left.get(), rays, nodeMask, tMin, hitMask, state);
  intersectPacketNode(node->right.get(), rays, nodeMask, tMin, hitMask, state);
}

PrimitivePacketHit4 BVH::intersectPacketHits(const Ray4& rays,
                                             const PrimitivePacketState4& states) const {
  if (!m_root) {
    return Composite::intersectPacketHits(rays, states);
  }

  PrimitivePacketHit4 result;
  std::array<double, Ray4::lanes> minDistances;
  minDistances.fill(std::numeric_limits<double>::infinity());
  uint16_t activeMask = 0;
  for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
    if (states[lane]) {
      activeMask |= static_cast<uint16_t>(1u << lane);
    }
  }
  intersectPacketHitNode(m_root.get(), rays, activeMask, minDistances, result, states);
  return result;
}

PrimitivePacketHit8 BVH::intersectPacketHits(const Ray8& rays,
                                             const PrimitivePacketState8& states) const {
  if (!m_root) {
    return Composite::intersectPacketHits(rays, states);
  }

  PrimitivePacketHit8 result;
  std::array<double, Ray8::lanes> minDistances;
  minDistances.fill(std::numeric_limits<double>::infinity());
  uint16_t activeMask = 0;
  for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
    if (states[lane]) {
      activeMask |= static_cast<uint16_t>(1u << lane);
    }
  }
  intersectPacketHitNode(m_root.get(), rays, activeMask, minDistances, result, states);
  return result;
}

void BVH::intersectPacketHitNode(const Node* node, const Ray4& rays, uint16_t activeMask,
                                 std::array<double, Ray4::lanes>& minDistances,
                                 PrimitivePacketHit4& result,
                                 const PrimitivePacketState4& states) const {
  if (!node || activeMask == 0) {
    return;
  }

  const uint16_t nodeMask = static_cast<uint16_t>(
    activeMask & static_cast<uint16_t>(core::simd::movemask(node->bbox.intersects4(rays))));
  if (!nodeMask) {
    return;
  }

  if (node->isLeaf()) {
    const PrimitivePacketState4 activeStates = activePacketStates(states, nodeMask);
    for (const auto& prim : node->primitives) {
      const PrimitivePacketHit4 candidate = prim->intersectPacketHits(rays, activeStates);
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        if ((nodeMask & (1u << lane)) == 0 || !candidate.hit(lane)) {
          continue;
        }

        const HitPoint& hitPoint = candidate.hitPoint(lane);
        if (hitPoint.distance() < minDistances[lane]) {
          result.setHit(lane, candidate.primitive(lane), hitPoint, candidate.scalarFallback(lane));
          minDistances[lane] = hitPoint.distance();
        }
      }
    }
    return;
  }

  intersectPacketHitNode(node->left.get(), rays, nodeMask, minDistances, result, states);
  intersectPacketHitNode(node->right.get(), rays, nodeMask, minDistances, result, states);
}

void BVH::intersectPacketHitNode(const Node* node, const Ray8& rays, uint16_t activeMask,
                                 std::array<double, Ray8::lanes>& minDistances,
                                 PrimitivePacketHit8& result,
                                 const PrimitivePacketState8& states) const {
  if (!node || activeMask == 0) {
    return;
  }

  uint16_t nodeMask = 0;
  for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
    if ((activeMask & (1u << lane)) && node->bbox.intersects(rays.rayd(lane))) {
      nodeMask |= static_cast<uint16_t>(1u << lane);
    }
  }
  if (!nodeMask) {
    return;
  }

  if (node->isLeaf()) {
    const PrimitivePacketState8 activeStates = activePacketStates(states, nodeMask);
    for (const auto& prim : node->primitives) {
      const PrimitivePacketHit8 candidate = prim->intersectPacketHits(rays, activeStates);
      for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
        if ((nodeMask & (1u << lane)) == 0 || !candidate.hit(lane)) {
          continue;
        }

        const HitPoint& hitPoint = candidate.hitPoint(lane);
        if (hitPoint.distance() < minDistances[lane]) {
          result.setHit(lane, candidate.primitive(lane), hitPoint, candidate.scalarFallback(lane));
          minDistances[lane] = hitPoint.distance();
        }
      }
    }
    return;
  }

  intersectPacketHitNode(node->left.get(), rays, nodeMask, minDistances, result, states);
  intersectPacketHitNode(node->right.get(), rays, nodeMask, minDistances, result, states);
}

#if RAYTRACER_SIMD_AVX
RayPacketIntersection8 BVH::intersectPacket(const Ray8& rays, render::State& state) const {
  if (!m_root) {
    return Primitive::intersectPacket(rays, state);
  }
  std::array<float, Ray8::lanes> tMin;
  tMin.fill(std::numeric_limits<float>::infinity());
  uint16_t hitMask = 0;
  constexpr uint16_t allActive = static_cast<uint16_t>((1u << Ray8::lanes) - 1u);
  intersectPacketNode(m_root.get(), rays, allActive, tMin, hitMask, state);
  RayPacketIntersection8 result;
  for (std::size_t i = 0; i < Ray8::lanes; ++i) {
    if (hitMask & (1u << i)) {
      result.setHit(i, tMin[i], tMin[i]);
    }
  }
  return result;
}

void BVH::intersectPacketNode(const Node* node, const Ray8& rays, uint16_t activeMask,
                              std::array<float, Ray8::lanes>& tMin, uint16_t& hitMask,
                              render::State& state) const {
  if (!node || activeMask == 0)
    return;

  uint16_t nodeMask = 0;
  for (std::size_t i = 0; i < Ray8::lanes; ++i) {
    if ((activeMask & (1u << i)) && node->bbox.intersects(rays.rayd(i))) {
      nodeMask |= static_cast<uint16_t>(1u << i);
    }
  }
  if (!nodeMask)
    return;

  if (node->isLeaf()) {
    for (const auto& prim : node->primitives) {
      const RayPacketIntersection8 r = prim->intersectPacket(rays, state);
      for (std::size_t i = 0; i < Ray8::lanes; ++i) {
        if ((nodeMask & (1u << i)) && r.hit(i)) {
          const float hitT = r.tNear[i] > 0.0f ? r.tNear[i] : r.tFar[i];
          if (hitT > 0.0f && hitT < tMin[i]) {
            tMin[i] = hitT;
            hitMask |= static_cast<uint16_t>(1u << i);
          }
        }
      }
    }
    return;
  }

  intersectPacketNode(node->left.get(), rays, nodeMask, tMin, hitMask, state);
  intersectPacketNode(node->right.get(), rays, nodeMask, tMin, hitMask, state);
}
#endif // RAYTRACER_SIMD_AVX
