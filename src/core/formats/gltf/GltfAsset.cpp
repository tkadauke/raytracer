#include "core/formats/gltf/GltfAsset.h"

#include <stdexcept>

namespace core::gltf {

  std::size_t componentTypeByteSize(ComponentType componentType) {
    switch (componentType) {
    case ComponentType::Int8:
    case ComponentType::Uint8:
      return 1;
    case ComponentType::Int16:
    case ComponentType::Uint16:
      return 2;
    case ComponentType::Uint32:
    case ComponentType::Float32:
      return 4;
    }
    throw std::logic_error("Unhandled glTF component type");
  }

  std::size_t accessorTypeComponentCount(AccessorType type) {
    switch (type) {
    case AccessorType::Scalar:
      return 1;
    case AccessorType::Vec2:
      return 2;
    case AccessorType::Vec3:
      return 3;
    case AccessorType::Vec4:
    case AccessorType::Mat2:
      return 4;
    case AccessorType::Mat3:
      return 9;
    case AccessorType::Mat4:
      return 16;
    }
    throw std::logic_error("Unhandled glTF accessor type");
  }

  std::size_t accessorElementByteSize(const Accessor& accessor) {
    return componentTypeByteSize(accessor.componentType) *
           accessorTypeComponentCount(accessor.type);
  }

  std::string toString(ComponentType componentType) {
    switch (componentType) {
    case ComponentType::Int8:
      return "BYTE";
    case ComponentType::Uint8:
      return "UNSIGNED_BYTE";
    case ComponentType::Int16:
      return "SHORT";
    case ComponentType::Uint16:
      return "UNSIGNED_SHORT";
    case ComponentType::Uint32:
      return "UNSIGNED_INT";
    case ComponentType::Float32:
      return "FLOAT";
    }
    throw std::logic_error("Unhandled glTF component type");
  }

  std::string toString(AccessorType type) {
    switch (type) {
    case AccessorType::Scalar:
      return "SCALAR";
    case AccessorType::Vec2:
      return "VEC2";
    case AccessorType::Vec3:
      return "VEC3";
    case AccessorType::Vec4:
      return "VEC4";
    case AccessorType::Mat2:
      return "MAT2";
    case AccessorType::Mat3:
      return "MAT3";
    case AccessorType::Mat4:
      return "MAT4";
    }
    throw std::logic_error("Unhandled glTF accessor type");
  }

}
