#pragma once

#include <cstdint>
#include <cstring>

namespace core::formats {

  inline std::uint32_t readUint16Le(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8u);
  }

  inline std::uint32_t readUint32Le(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8u) |
           (static_cast<std::uint32_t>(data[2]) << 16u) |
           (static_cast<std::uint32_t>(data[3]) << 24u);
  }

  inline float readFloat32Le(const std::uint8_t* data) {
    const std::uint32_t bits = readUint32Le(data);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }

}
