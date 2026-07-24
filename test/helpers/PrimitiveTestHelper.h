#pragma once

#include "core/math/RayPacket.h"
#include "core/math/Vector.h"
#include "render/State.h"
#include "render/primitives/Box.h"
#include "render/primitives/Primitive.h"

#include <array>

namespace test::helpers {
  inline render::Box* unitBox() {
    static render::Box box(Vector3d::null, Vector3d::one);
    return &box;
  }

  // Bundles the per-lane State array together with the PrimitivePacketState4
  // pointer array so packet-intersection tests can be written as:
  //
  //   PacketStates4 ps;
  //   auto result = prim.intersectPacketHits(rays, ps.states);
  //   EXPECT_EQ(1, ps.lanes[0].intersectionHits);
  struct PacketStates4 {
    std::array<render::State, Ray4::lanes> lanes;
    render::PrimitivePacketState4 states;

    PacketStates4()
        : states{&lanes[0], &lanes[1], &lanes[2], &lanes[3]} {}
  };

  struct PacketStates8 {
    std::array<render::State, Ray8::lanes> lanes;
    render::PrimitivePacketState8 states;

    PacketStates8()
        : states{&lanes[0], &lanes[1], &lanes[2], &lanes[3],
                 &lanes[4], &lanes[5], &lanes[6], &lanes[7]} {}
  };
}
