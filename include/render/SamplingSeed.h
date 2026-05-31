#pragma once

#include <cstdint>

namespace render {
  /**
    * Stable seed derivation for stochastic rendering.
    *
    * Ownership is hierarchical:
    *  - render seed: caller/test supplied root for one frame.
    *  - tile seed: derived from the render seed and deterministic tile index.
    *  - pixel seed: derived from the tile seed and framebuffer coordinates.
    *  - sample seed: derived from the pixel seed and per-pixel sample index.
    *
    * Current samplers still use pre-generated 2D sets plus sampleIndex lookups;
    * future path-tracing dimensions can use sampleSeed() as their per-path root.
    */
  struct SamplingSeed {
    static std::uint64_t mix(std::uint64_t value) noexcept {
      value += 0x9e3779b97f4a7c15ull;
      value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
      value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
      return value ^ (value >> 31u);
    }

    static std::uint64_t tileSeed(std::uint64_t renderSeed, std::uint64_t tileIndex) noexcept {
      return mix(renderSeed ^ mix(tileIndex));
    }

    static std::uint64_t pixelSeed(std::uint64_t tileSeed, int x, int y) noexcept {
      const auto ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x));
      const auto uy = static_cast<std::uint64_t>(static_cast<std::uint32_t>(y));
      return mix(tileSeed ^ (ux << 32u) ^ uy);
    }

    static std::uint64_t sampleSeed(std::uint64_t pixelSeed, std::uint64_t sampleIndex) noexcept {
      return mix(pixelSeed ^ mix(sampleIndex));
    }
  };
}
