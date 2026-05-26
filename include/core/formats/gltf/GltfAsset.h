#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace core::gltf {

  enum class ComponentType {
    Int8 = 5120,
    Uint8 = 5121,
    Int16 = 5122,
    Uint16 = 5123,
    Uint32 = 5125,
    Float32 = 5126
  };

  enum class AccessorType { Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4 };

  struct Buffer {
    std::string uri;
    std::size_t byteLength = 0;
    std::vector<std::uint8_t> data;
    std::filesystem::path resolvedPath;
    std::string identity;

    [[nodiscard]] bool hasData() const {
      return !data.empty() || byteLength == 0;
    }
  };

  struct BufferView {
    std::size_t buffer = 0;
    std::size_t byteOffset = 0;
    std::size_t byteLength = 0;
    std::optional<std::size_t> byteStride;
    std::optional<int> target;
  };

  struct Accessor {
    std::optional<std::size_t> bufferView;
    std::size_t byteOffset = 0;
    ComponentType componentType = ComponentType::Float32;
    bool normalized = false;
    std::size_t count = 0;
    AccessorType type = AccessorType::Scalar;
  };

  struct Image {
    std::string uri;
    std::string mimeType;
    std::optional<std::size_t> bufferView;
    std::vector<std::uint8_t> data;
    std::filesystem::path resolvedPath;
    std::string identity;

    [[nodiscard]] bool hasData() const {
      return !data.empty();
    }
  };

  struct Asset {
    std::string version;
    std::string generator;
    std::vector<Buffer> buffers;
    std::vector<BufferView> bufferViews;
    std::vector<Accessor> accessors;
    std::vector<Image> images;
  };

  [[nodiscard]] std::size_t componentTypeByteSize(ComponentType componentType);
  [[nodiscard]] std::size_t accessorTypeComponentCount(AccessorType type);
  [[nodiscard]] std::size_t accessorElementByteSize(const Accessor& accessor);
  [[nodiscard]] std::string toString(ComponentType componentType);
  [[nodiscard]] std::string toString(AccessorType type);

}
