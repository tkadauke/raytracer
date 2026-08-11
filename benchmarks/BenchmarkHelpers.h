#pragma once

#include "core/math/RayPacket.h"
#include "core/math/Vector.h"

#include <array>
#include <random>
#include <vector>

// Shared benchmark helpers to avoid repeating the same small utilities
// across multiple benchmark translation units.

template<typename RayType>
std::vector<Ray4> packetize(const std::vector<RayType>& rays) {
  std::vector<Ray4> packets;
  packets.reserve(rays.size() / Ray4::lanes);
  for (std::size_t i = 0; i < rays.size(); i += Ray4::lanes) {
    packets.emplace_back(std::array<RayType, 4>{rays[i], rays[i + 1], rays[i + 2], rays[i + 3]});
  }
  return packets;
}

// Draws a random direction from `component` and normalizes it, falling back
// to a fixed axis when the draw is degenerate (near-zero length).
inline Vector3d randomUnitDirection(std::mt19937& rng,
                                    std::uniform_real_distribution<double>& component) {
  Vector3d d(component(rng), component(rng), component(rng));
  if (d.length() < 1e-6)
    d = Vector3d(1, 0, 0);
  return d.normalized();
}
