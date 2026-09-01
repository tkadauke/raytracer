#pragma once

#include "core/math/Vector.h"

#include <QByteArray>

#include <array>
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

  template<std::size_t Size>
  inline std::array<float, Size> readFloat32ArrayLe(const std::uint8_t* data) {
    std::array<float, Size> result{};
    for (std::size_t i = 0; i != Size; ++i)
      result[i] = readFloat32Le(data + i * sizeof(float));
    return result;
  }

  inline Vector2d readVector2fLe(const std::uint8_t* data) {
    return Vector2d(readFloat32ArrayLe<2>(data));
  }

  inline Vector3d readVector3fLe(const std::uint8_t* data) {
    return Vector3d(readFloat32ArrayLe<3>(data));
  }

  // Unchecked little-endian reads from a QByteArray at a byte offset. Callers
  // are responsible for verifying the offset lies within bounds beforehand.
  inline std::uint16_t readUint16Le(const QByteArray& bytes, qsizetype offset) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData() + offset);
    return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
  }

  inline std::uint32_t readUint32Le(const QByteArray& bytes, qsizetype offset) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData() + offset);
    return static_cast<std::uint32_t>(data[0] | (data[1] << 8) | (data[2] << 16) |
                                      (data[3] << 24));
  }

  inline float readFloat32Le(const QByteArray& bytes, qsizetype offset) {
    const std::uint32_t bits = readUint32Le(bytes, offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }

  template<std::size_t Size>
  inline std::array<float, Size> readFloat32ArrayLe(const QByteArray& bytes, qsizetype offset) {
    std::array<float, Size> result{};
    for (std::size_t i = 0; i != Size; ++i)
      result[i] = readFloat32Le(bytes, offset + static_cast<qsizetype>(i * sizeof(float)));
    return result;
  }

  inline Vector2d readVector2fLe(const QByteArray& bytes, qsizetype offset) {
    return Vector2d(readFloat32ArrayLe<2>(bytes, offset));
  }

  inline Vector3d readVector3fLe(const QByteArray& bytes, qsizetype offset) {
    return Vector3d(readFloat32ArrayLe<3>(bytes, offset));
  }

}
