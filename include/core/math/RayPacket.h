#pragma once

#include "core/math/Ray.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

template<std::size_t Lanes, std::size_t Alignment>
class alignas(Alignment) RayPacket {
public:
  static constexpr std::size_t lanes = Lanes;

  using LaneArray = std::array<float, Lanes>;

  RayPacket() = default;

  explicit RayPacket(const std::array<Rayf, Lanes>& rays) {
    for (std::size_t i = 0; i != Lanes; ++i) {
      set(i, rays[i]);
    }
  }

  explicit RayPacket(const std::array<Rayd, Lanes>& rays) {
    for (std::size_t i = 0; i != Lanes; ++i) {
      set(i, rays[i]);
    }
  }

  void set(std::size_t lane, const Rayf& ray) {
    originX[lane] = ray.origin().x();
    originY[lane] = ray.origin().y();
    originZ[lane] = ray.origin().z();
    directionX[lane] = ray.direction().x();
    directionY[lane] = ray.direction().y();
    directionZ[lane] = ray.direction().z();
  }

  void set(std::size_t lane, const Rayd& ray) {
    originX[lane] = static_cast<float>(ray.origin().x());
    originY[lane] = static_cast<float>(ray.origin().y());
    originZ[lane] = static_cast<float>(ray.origin().z());
    directionX[lane] = static_cast<float>(ray.direction().x());
    directionY[lane] = static_cast<float>(ray.direction().y());
    directionZ[lane] = static_cast<float>(ray.direction().z());
  }

  [[nodiscard]] Rayf ray(std::size_t lane) const {
    return Rayf(
      Vector3f(originX[lane], originY[lane], originZ[lane]),
      Vector3f(directionX[lane], directionY[lane], directionZ[lane])
    );
  }

  [[nodiscard]] Rayd rayd(std::size_t lane) const {
    return Rayd(
      Vector3d(originX[lane], originY[lane], originZ[lane]),
      Vector3d(directionX[lane], directionY[lane], directionZ[lane])
    );
  }

  alignas(Alignment) LaneArray originX{};
  alignas(Alignment) LaneArray originY{};
  alignas(Alignment) LaneArray originZ{};
  alignas(Alignment) LaneArray directionX{};
  alignas(Alignment) LaneArray directionY{};
  alignas(Alignment) LaneArray directionZ{};
};

template<std::size_t Lanes, std::size_t Alignment>
class alignas(Alignment) RayPacketIntersection {
public:
  static constexpr std::size_t lanes = Lanes;

  RayPacketIntersection() {
    tNear.fill(std::numeric_limits<float>::infinity());
    tFar.fill(std::numeric_limits<float>::infinity());
  }

  void setHit(std::size_t lane, float nearDistance, float farDistance) {
    hitMask |= static_cast<std::uint16_t>(1u << lane);
    tNear[lane] = nearDistance;
    tFar[lane] = farDistance;
  }

  [[nodiscard]] bool hit(std::size_t lane) const {
    return (hitMask & static_cast<std::uint16_t>(1u << lane)) != 0;
  }

  std::uint16_t hitMask = 0;
  alignas(Alignment) std::array<float, Lanes> tNear{};
  alignas(Alignment) std::array<float, Lanes> tFar{};
};

using Ray4 = RayPacket<4, 16>;
using Ray8 = RayPacket<8, 32>;
using RayPacketIntersection4 = RayPacketIntersection<4, 16>;
using RayPacketIntersection8 = RayPacketIntersection<8, 32>;

