#pragma once

#include "core/math/RayPacket.h"

#include <array>
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
