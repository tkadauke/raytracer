#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
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

  struct Sampler {
    std::optional<int> magFilter;
    std::optional<int> minFilter;
    int wrapS = 10497;
    int wrapT = 10497;
  };

  struct Texture {
    std::optional<std::size_t> sampler;
    std::optional<std::size_t> source;
  };

  struct TextureInfo {
    std::size_t index = 0;
    int texCoord = 0;
  };

  struct Material {
    std::string name;
    std::array<double, 4> baseColorFactor{1.0, 1.0, 1.0, 1.0};
    std::optional<TextureInfo> baseColorTexture;
    std::optional<TextureInfo> metallicRoughnessTexture;
    std::optional<double> metallicFactor;
    std::optional<double> roughnessFactor;
    std::string alphaMode = "OPAQUE";
    double alphaCutoff = 0.5;
    bool doubleSided = false;
    std::vector<std::string> unsupportedFeatures;
  };

  struct MeshPrimitive {
    std::map<std::string, std::size_t> attributes;
    std::optional<std::size_t> indices;
    std::optional<std::size_t> material;
    int mode = 4;
  };

  struct Mesh {
    std::string name;
    std::vector<MeshPrimitive> primitives;
  };

  struct Node {
    std::string name;
    std::vector<std::size_t> children;
    std::optional<std::size_t> mesh;
    std::optional<std::size_t> camera;
    std::optional<std::size_t> punctualLight;
    std::optional<std::array<double, 16>> matrix;
    std::array<double, 3> translation{0.0, 0.0, 0.0};
    std::array<double, 4> rotation{0.0, 0.0, 0.0, 1.0};
    std::array<double, 3> scale{1.0, 1.0, 1.0};
  };

  enum class CameraType { Perspective, Orthographic };

  struct PerspectiveCamera {
    std::optional<double> aspectRatio;
    double yfov = 0.0;
    double znear = 0.0;
    std::optional<double> zfar;
  };

  struct OrthographicCamera {
    double xmag = 0.0;
    double ymag = 0.0;
    double znear = 0.0;
    double zfar = 0.0;
  };

  struct Camera {
    std::string name;
    CameraType type = CameraType::Perspective;
    PerspectiveCamera perspective;
    OrthographicCamera orthographic;
  };

  enum class PunctualLightType { Directional, Point, Spot };

  struct SpotLight {
    double innerConeAngle = 0.0;
    double outerConeAngle = 0.7853981633974483;
  };

  struct PunctualLight {
    std::string name;
    PunctualLightType type = PunctualLightType::Point;
    std::array<double, 3> color{1.0, 1.0, 1.0};
    double intensity = 1.0;
    std::optional<double> range;
    SpotLight spot;
  };

  struct Scene {
    std::string name;
    std::vector<std::size_t> nodes;
  };

  struct AnimationSampler {
    std::size_t input = 0;
    std::size_t output = 0;
    std::string interpolation = "LINEAR";
  };

  struct AnimationChannelTarget {
    std::optional<std::size_t> node;
    std::string path;
  };

  struct AnimationChannel {
    std::size_t sampler = 0;
    AnimationChannelTarget target;
  };

  struct Animation {
    std::string name;
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
  };

  struct Asset {
    std::string version;
    std::string generator;
    std::vector<Buffer> buffers;
    std::vector<BufferView> bufferViews;
    std::vector<Accessor> accessors;
    std::vector<Image> images;
    std::vector<Sampler> samplers;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<Camera> cameras;
    std::vector<PunctualLight> punctualLights;
    std::vector<Node> nodes;
    std::vector<Scene> scenes;
    std::vector<Animation> animations;
    std::optional<std::size_t> defaultScene;
  };

  [[nodiscard]] std::size_t componentTypeByteSize(ComponentType componentType);
  [[nodiscard]] std::size_t accessorTypeComponentCount(AccessorType type);
  [[nodiscard]] std::size_t accessorElementByteSize(const Accessor& accessor);
  [[nodiscard]] std::string toString(ComponentType componentType);
  [[nodiscard]] std::string toString(AccessorType type);

}
