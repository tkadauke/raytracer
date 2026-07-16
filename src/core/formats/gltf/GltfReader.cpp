#include "core/formats/gltf/GltfReader.h"
#include "core/json/JsonValue.h"
#include "core/util/StringUtil.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>
#include <QUrl>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>

using core::util::startsWith;

namespace fs = std::filesystem;

namespace core::gltf {
  namespace {
    constexpr std::uint32_t glbMagic = 0x46546c67;
    constexpr std::uint32_t glbJsonChunk = 0x4e4f534a;
    constexpr std::uint32_t glbBinaryChunk = 0x004e4942;

    std::string jsonPath(const std::string& base, std::size_t index, const std::string& name = {}) {
      std::ostringstream out;
      out << base << '[' << index << ']';
      if (!name.empty())
        out << '.' << name;
      return out.str();
    }

    std::vector<std::uint8_t> readAllBytes(const fs::path& path, Diagnostics& diagnostics) {
      std::ifstream input(path, std::ios::binary);
      if (!input) {
        diagnostics.error(DiagnosticCode::IoError, path.generic_string(), "Unable to open file");
        return {};
      }

      input.seekg(0, std::ios::end);
      const std::streamoff size = input.tellg();
      if (size < 0) {
        diagnostics.error(DiagnosticCode::IoError, path.generic_string(), "Unable to measure file");
        return {};
      }
      input.seekg(0, std::ios::beg);

      std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
      if (!bytes.empty())
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
      if (!input && !bytes.empty()) {
        diagnostics.error(DiagnosticCode::IoError, path.generic_string(), "Unable to read file");
        return {};
      }
      return bytes;
    }

    std::string bytesToString(const std::vector<std::uint8_t>& bytes) {
      return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    std::uint32_t readUint32Le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
      return static_cast<std::uint32_t>(bytes[offset]) |
             (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
             (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
             (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
    }

    std::optional<std::size_t> unsignedInteger(const QJsonObject& object, const char* name,
                                               const std::string& path, Diagnostics& diagnostics,
                                               bool required = true) {
      const QJsonValue value = object.value(name);
      if (value.isUndefined()) {
        if (required)
          diagnostics.error(DiagnosticCode::MissingRequiredProperty, path + "." + name,
                            "Missing required property");
        return std::nullopt;
      }
      if (!value.isDouble()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Expected an unsigned integer");
        return std::nullopt;
      }

      const double number = value.toDouble();
      const auto integer = static_cast<unsigned long long>(number);
      if (number < 0.0 || number != static_cast<double>(integer) ||
          integer > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Expected an unsigned integer");
        return std::nullopt;
      }
      return static_cast<std::size_t>(integer);
    }

    std::optional<int> integer(const QJsonObject& object, const char* name, const std::string& path,
                               Diagnostics& diagnostics, bool required = true) {
      const auto number = unsignedInteger(object, name, path, diagnostics, required);
      if (!number)
        return std::nullopt;
      if (*number > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Integer is out of range");
        return std::nullopt;
      }
      return static_cast<int>(*number);
    }

    std::optional<std::string> stringProperty(const QJsonObject& object, const char* name,
                                              const std::string& path, Diagnostics& diagnostics,
                                              bool required = false) {
      const QJsonValue value = object.value(name);
      if (value.isUndefined()) {
        if (required)
          diagnostics.error(DiagnosticCode::MissingRequiredProperty, path + "." + name,
                            "Missing required property");
        return std::nullopt;
      }
      if (!value.isString()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Expected a string");
        return std::nullopt;
      }
      return value.toString().toStdString();
    }

    template<std::size_t Size>
    bool numberArray(const QJsonObject& object, const char* name, const std::string& path,
                     Diagnostics& diagnostics, std::array<double, Size>* result) {
      const QJsonValue value = object.value(name);
      if (value.isUndefined())
        return false;

      const auto values = core::json::requireNumberArray<Size>(
        value, "Expected an array", "Unexpected array length", "Expected a number",
        [&](std::optional<int> index, const char* message) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType,
                            index ? path + "." + name + "[" + std::to_string(*index) + "]"
                                  : path + "." + name,
                            message);
        });
      if (!values)
        return false;
      *result = *values;
      return true;
    }

    std::vector<std::size_t> indexArray(const QJsonObject& object, const char* name,
                                        const std::string& path, Diagnostics& diagnostics) {
      std::vector<std::size_t> result;
      const QJsonValue value = object.value(name);
      if (value.isUndefined())
        return result;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Expected an array");
        return result;
      }

      const QJsonArray array = value.toArray();
      result.reserve(static_cast<std::size_t>(array.size()));
      for (int i = 0; i < array.size(); ++i) {
        if (!array.at(i).isDouble()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType,
                            path + "." + name + "[" + std::to_string(i) + "]",
                            "Expected an unsigned integer");
          continue;
        }
        const double number = array.at(i).toDouble();
        const auto integer = static_cast<unsigned long long>(number);
        if (number < 0.0 || number != static_cast<double>(integer) ||
            integer > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType,
                            path + "." + name + "[" + std::to_string(i) + "]",
                            "Expected an unsigned integer");
          continue;
        }
        result.push_back(static_cast<std::size_t>(integer));
      }
      return result;
    }

    std::map<std::string, std::size_t> attributeMap(const QJsonObject& object, const char* name,
                                                    const std::string& path,
                                                    Diagnostics& diagnostics) {
      std::map<std::string, std::size_t> result;
      const QJsonValue value = object.value(name);
      if (value.isUndefined())
        return result;
      if (!value.isObject()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Expected an object");
        return result;
      }

      const QJsonObject attributes = value.toObject();
      for (auto it = attributes.begin(); it != attributes.end(); ++it) {
        if (!it.value().isDouble()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType,
                            path + "." + name + "." + it.key().toStdString(),
                            "Expected an unsigned integer");
          continue;
        }
        const double number = it.value().toDouble();
        const auto integer = static_cast<unsigned long long>(number);
        if (number < 0.0 || number != static_cast<double>(integer) ||
            integer > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType,
                            path + "." + name + "." + it.key().toStdString(),
                            "Expected an unsigned integer");
          continue;
        }
        result[it.key().toStdString()] = static_cast<std::size_t>(integer);
      }
      return result;
    }

    std::optional<double> numberProperty(const QJsonObject& object, const char* name,
                                         const std::string& path, Diagnostics& diagnostics,
                                         bool required = false) {
      const QJsonValue value = object.value(name);
      if (value.isUndefined()) {
        if (required)
          diagnostics.error(DiagnosticCode::MissingRequiredProperty, path + "." + name,
                            "Missing required property");
        return std::nullopt;
      }
      if (!value.isDouble()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Expected a number");
        return std::nullopt;
      }
      return value.toDouble();
    }

    std::optional<bool> boolProperty(const QJsonObject& object, const char* name,
                                     const std::string& path, Diagnostics& diagnostics,
                                     bool required = false) {
      const QJsonValue value = object.value(name);
      if (value.isUndefined()) {
        if (required)
          diagnostics.error(DiagnosticCode::MissingRequiredProperty, path + "." + name,
                            "Missing required property");
        return std::nullopt;
      }
      if (!value.isBool()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Expected a boolean");
        return std::nullopt;
      }
      return value.toBool();
    }

    std::optional<TextureInfo> textureInfoProperty(const QJsonObject& object, const char* name,
                                                   const std::string& path,
                                                   Diagnostics& diagnostics) {
      const QJsonValue value = object.value(name);
      if (value.isUndefined())
        return std::nullopt;
      if (!value.isObject()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, path + "." + name,
                          "Expected an object");
        return std::nullopt;
      }

      const std::string texturePath = path + "." + name;
      const QJsonObject textureObject = value.toObject();
      const auto index = unsignedInteger(textureObject, "index", texturePath, diagnostics, true);
      if (!index)
        return std::nullopt;

      TextureInfo info;
      info.index = *index;
      if (auto texCoord = integer(textureObject, "texCoord", texturePath, diagnostics, false))
        info.texCoord = *texCoord;
      return info;
    }

    std::optional<ComponentType> componentTypeFromInt(int value) {
      switch (value) {
      case 5120:
        return ComponentType::Int8;
      case 5121:
        return ComponentType::Uint8;
      case 5122:
        return ComponentType::Int16;
      case 5123:
        return ComponentType::Uint16;
      case 5125:
        return ComponentType::Uint32;
      case 5126:
        return ComponentType::Float32;
      default:
        return std::nullopt;
      }
    }

    std::optional<AccessorType> accessorTypeFromString(const std::string& value) {
      if (value == "SCALAR")
        return AccessorType::Scalar;
      if (value == "VEC2")
        return AccessorType::Vec2;
      if (value == "VEC3")
        return AccessorType::Vec3;
      if (value == "VEC4")
        return AccessorType::Vec4;
      if (value == "MAT2")
        return AccessorType::Mat2;
      if (value == "MAT3")
        return AccessorType::Mat3;
      if (value == "MAT4")
        return AccessorType::Mat4;
      return std::nullopt;
    }

    std::optional<CameraType> cameraTypeFromString(const std::string& value) {
      if (value == "perspective")
        return CameraType::Perspective;
      if (value == "orthographic")
        return CameraType::Orthographic;
      return std::nullopt;
    }

    std::optional<PunctualLightType> punctualLightTypeFromString(const std::string& value) {
      if (value == "directional")
        return PunctualLightType::Directional;
      if (value == "point")
        return PunctualLightType::Point;
      if (value == "spot")
        return PunctualLightType::Spot;
      return std::nullopt;
    }

    std::optional<std::vector<std::uint8_t>>
    decodeDataUri(const std::string& uri, const std::string& path, Diagnostics& diagnostics) {
      if (!startsWith(uri, "data:"))
        return std::nullopt;

      const std::size_t comma = uri.find(',');
      if (comma == std::string::npos) {
        diagnostics.error(DiagnosticCode::InvalidUri, path, "Malformed data URI", uri);
        return std::vector<std::uint8_t>{};
      }

      const std::string metadata = uri.substr(5, comma - 5);
      const std::string payload = uri.substr(comma + 1);
      QByteArray decoded;
      if (metadata.find(";base64") != std::string::npos) {
        decoded =
          QByteArray::fromBase64(QByteArray(payload.data(), static_cast<int>(payload.size())));
      } else {
        decoded =
          QUrl::fromPercentEncoding(QByteArray(payload.data(), static_cast<int>(payload.size())))
            .toUtf8();
      }

      return std::vector<std::uint8_t>(decoded.begin(), decoded.end());
    }

    std::string uriToPath(const std::string& uri) {
      return QUrl::fromPercentEncoding(QByteArray(uri.data(), static_cast<int>(uri.size())))
        .toStdString();
    }

    std::vector<std::uint8_t> resolveUriBytes(const std::string& uri, const fs::path& currentFile,
                                              const AssetResolver& resolver,
                                              const std::string& path, Diagnostics& diagnostics,
                                              fs::path* resolvedPath, std::string* identity) {
      if (auto embedded = decodeDataUri(uri, path, diagnostics))
        return *embedded;

      if (startsWith(uri, "http://") || startsWith(uri, "https://")) {
        diagnostics.error(DiagnosticCode::UnsupportedValue, path,
                          "Remote URIs are not supported by the local asset resolver", uri);
        return {};
      }

      try {
        const ResolvedAsset resolved = resolver.resolve(uriToPath(uri), currentFile);
        if (resolvedPath)
          *resolvedPath = resolved.path;
        if (identity)
          *identity = resolved.identity;
        return readAllBytes(resolved.path, diagnostics);
      } catch (const AssetResolutionError& error) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostic.code = DiagnosticCode::AssetResolutionFailed;
        diagnostic.path = path;
        diagnostic.message = error.what();
        diagnostic.reference = uri;
        for (const fs::path& root : error.searchedRoots())
          diagnostic.searchedRoots.push_back(root.generic_string());
        diagnostics.add(std::move(diagnostic));
        return {};
      }
    }

    void validateBufferLength(const Buffer& buffer, std::size_t index, Diagnostics& diagnostics) {
      if (!buffer.hasData())
        return;
      if (buffer.data.size() < buffer.byteLength) {
        diagnostics.error(DiagnosticCode::BufferLengthMismatch, jsonPath("buffers", index, "uri"),
                          "Resolved buffer is shorter than byteLength", buffer.uri);
      }
    }

    void validateBufferViews(const Asset& asset, Diagnostics& diagnostics) {
      for (std::size_t i = 0; i < asset.bufferViews.size(); ++i) {
        const BufferView& view = asset.bufferViews[i];
        const std::string path = jsonPath("bufferViews", i);
        if (view.buffer >= asset.buffers.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference, path + ".buffer",
                            "bufferView references a missing buffer");
          continue;
        }
        if (view.byteStride &&
            (*view.byteStride < 4 || *view.byteStride > 252 || (*view.byteStride % 4) != 0)) {
          diagnostics.error(DiagnosticCode::InvalidAccessor, path + ".byteStride",
                            "byteStride must be a multiple of 4 between 4 and 252");
        }
        const Buffer& buffer = asset.buffers[view.buffer];
        if (view.byteOffset > buffer.byteLength ||
            view.byteLength > buffer.byteLength - view.byteOffset) {
          diagnostics.error(DiagnosticCode::BufferViewOutOfBounds, path + ".byteLength",
                            "bufferView range exceeds its buffer byteLength");
        }
      }
    }

    void validateAccessors(const Asset& asset, Diagnostics& diagnostics) {
      for (std::size_t i = 0; i < asset.accessors.size(); ++i) {
        const Accessor& accessor = asset.accessors[i];
        const std::string path = jsonPath("accessors", i);
        if (!accessor.bufferView)
          continue;
        if (*accessor.bufferView >= asset.bufferViews.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference, path + ".bufferView",
                            "accessor references a missing bufferView");
          continue;
        }

        const BufferView& view = asset.bufferViews[*accessor.bufferView];
        const std::size_t elementSize = accessorElementByteSize(accessor);
        const std::size_t stride = view.byteStride.value_or(elementSize);
        if (stride < elementSize) {
          diagnostics.error(DiagnosticCode::InvalidAccessor, path + ".bufferView",
                            "accessor stride is smaller than one element");
          continue;
        }
        if (accessor.count == 0)
          continue;

        const std::size_t finalElementOffset = (accessor.count - 1) * stride;
        if (accessor.byteOffset > view.byteLength ||
            finalElementOffset > view.byteLength - accessor.byteOffset ||
            elementSize > view.byteLength - accessor.byteOffset - finalElementOffset) {
          diagnostics.error(DiagnosticCode::InvalidAccessor, path + ".count",
                            "accessor range exceeds its bufferView byteLength");
        }
      }
    }

    void validateImages(const Asset& asset, Diagnostics& diagnostics) {
      for (std::size_t i = 0; i < asset.images.size(); ++i) {
        const Image& image = asset.images[i];
        const std::string path = jsonPath("images", i);
        if (!image.uri.empty() && image.bufferView) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path,
                            "image must not define both uri and bufferView");
        }
        if (image.uri.empty() && !image.bufferView) {
          diagnostics.error(DiagnosticCode::MissingRequiredProperty, path,
                            "image requires either uri or bufferView");
        }
        if (image.bufferView && *image.bufferView >= asset.bufferViews.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference, path + ".bufferView",
                            "image references a missing bufferView");
        }
      }
    }

    void validateTexturesAndMaterials(const Asset& asset, Diagnostics& diagnostics) {
      for (std::size_t i = 0; i < asset.textures.size(); ++i) {
        const Texture& texture = asset.textures[i];
        const std::string path = jsonPath("textures", i);
        if (texture.sampler && *texture.sampler >= asset.samplers.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference, path + ".sampler",
                            "texture references a missing sampler");
        }
        if (texture.source && *texture.source >= asset.images.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference, path + ".source",
                            "texture references a missing image");
        }
      }

      for (std::size_t i = 0; i < asset.materials.size(); ++i) {
        const Material& material = asset.materials[i];
        const std::string path = jsonPath("materials", i) + ".pbrMetallicRoughness";
        if (material.baseColorTexture &&
            material.baseColorTexture->index >= asset.textures.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference, path + ".baseColorTexture.index",
                            "material references a missing texture");
        }
        if (material.metallicRoughnessTexture &&
            material.metallicRoughnessTexture->index >= asset.textures.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference,
                            path + ".metallicRoughnessTexture.index",
                            "material references a missing texture");
        }
      }
    }

    std::vector<std::uint8_t> bytesFromBufferView(const Asset& asset, std::size_t bufferView) {
      const BufferView& view = asset.bufferViews[bufferView];
      const Buffer& buffer = asset.buffers[view.buffer];
      if (buffer.data.size() < view.byteOffset + view.byteLength)
        return {};
      return {buffer.data.begin() + static_cast<std::ptrdiff_t>(view.byteOffset),
              buffer.data.begin() + static_cast<std::ptrdiff_t>(view.byteOffset + view.byteLength)};
    }

    void parseBuffers(const QJsonObject& root, Asset& asset, const fs::path& currentFile,
                      const AssetResolver& resolver, Diagnostics& diagnostics,
                      const std::vector<std::uint8_t>& glbBinaryChunk) {
      const QJsonValue value = root.value("buffers");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "buffers", "Expected an array");
        return;
      }

      const QJsonArray buffers = value.toArray();
      asset.buffers.reserve(static_cast<std::size_t>(buffers.size()));
      for (int i = 0; i < buffers.size(); ++i) {
        const std::string path = jsonPath("buffers", static_cast<std::size_t>(i));
        if (!buffers.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }
        const QJsonObject object = buffers.at(i).toObject();
        Buffer buffer;
        if (auto byteLength = unsignedInteger(object, "byteLength", path, diagnostics))
          buffer.byteLength = *byteLength;
        if (auto uri = stringProperty(object, "uri", path, diagnostics))
          buffer.uri = *uri;

        if (!buffer.uri.empty()) {
          buffer.data = resolveUriBytes(buffer.uri, currentFile, resolver, path + ".uri",
                                        diagnostics, &buffer.resolvedPath, &buffer.identity);
        } else if (i == 0 && !glbBinaryChunk.empty()) {
          buffer.data = glbBinaryChunk;
        }

        asset.buffers.push_back(std::move(buffer));
        validateBufferLength(asset.buffers.back(), static_cast<std::size_t>(i), diagnostics);
      }
    }

    void parseBufferViews(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("bufferViews");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "bufferViews", "Expected an array");
        return;
      }

      const QJsonArray views = value.toArray();
      asset.bufferViews.reserve(static_cast<std::size_t>(views.size()));
      for (int i = 0; i < views.size(); ++i) {
        const std::string path = jsonPath("bufferViews", static_cast<std::size_t>(i));
        if (!views.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }
        const QJsonObject object = views.at(i).toObject();
        BufferView view;
        if (auto buffer = unsignedInteger(object, "buffer", path, diagnostics))
          view.buffer = *buffer;
        if (auto byteOffset = unsignedInteger(object, "byteOffset", path, diagnostics, false))
          view.byteOffset = *byteOffset;
        if (auto byteLength = unsignedInteger(object, "byteLength", path, diagnostics))
          view.byteLength = *byteLength;
        view.byteStride = unsignedInteger(object, "byteStride", path, diagnostics, false);
        view.target = integer(object, "target", path, diagnostics, false);
        asset.bufferViews.push_back(view);
      }
    }

    void parseAccessors(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("accessors");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "accessors", "Expected an array");
        return;
      }

      const QJsonArray accessors = value.toArray();
      asset.accessors.reserve(static_cast<std::size_t>(accessors.size()));
      for (int i = 0; i < accessors.size(); ++i) {
        const std::string path = jsonPath("accessors", static_cast<std::size_t>(i));
        if (!accessors.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }
        const QJsonObject object = accessors.at(i).toObject();
        Accessor accessor;
        accessor.bufferView = unsignedInteger(object, "bufferView", path, diagnostics, false);
        if (auto byteOffset = unsignedInteger(object, "byteOffset", path, diagnostics, false))
          accessor.byteOffset = *byteOffset;
        if (auto component = integer(object, "componentType", path, diagnostics)) {
          if (auto type = componentTypeFromInt(*component)) {
            accessor.componentType = *type;
          } else {
            diagnostics.error(DiagnosticCode::InvalidAccessor, path + ".componentType",
                              "Unsupported accessor componentType");
          }
        }
        const QJsonValue normalized = object.value("normalized");
        if (normalized.isBool()) {
          accessor.normalized = normalized.toBool();
        } else if (!normalized.isUndefined()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path + ".normalized",
                            "Expected a boolean");
        }
        if (auto count = unsignedInteger(object, "count", path, diagnostics))
          accessor.count = *count;
        if (auto typeName = stringProperty(object, "type", path, diagnostics, true)) {
          if (auto type = accessorTypeFromString(*typeName)) {
            accessor.type = *type;
          } else {
            diagnostics.error(DiagnosticCode::InvalidAccessor, path + ".type",
                              "Unsupported accessor type");
          }
        }
        asset.accessors.push_back(accessor);
      }
    }

    void parseImages(const QJsonObject& root, Asset& asset, const fs::path& currentFile,
                     const AssetResolver& resolver, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("images");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "images", "Expected an array");
        return;
      }

      const QJsonArray images = value.toArray();
      asset.images.reserve(static_cast<std::size_t>(images.size()));
      for (int i = 0; i < images.size(); ++i) {
        const std::string path = jsonPath("images", static_cast<std::size_t>(i));
        if (!images.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }
        const QJsonObject object = images.at(i).toObject();
        Image image;
        if (auto uri = stringProperty(object, "uri", path, diagnostics))
          image.uri = *uri;
        if (auto mimeType = stringProperty(object, "mimeType", path, diagnostics))
          image.mimeType = *mimeType;
        image.bufferView = unsignedInteger(object, "bufferView", path, diagnostics, false);
        if (!image.uri.empty()) {
          image.data = resolveUriBytes(image.uri, currentFile, resolver, path + ".uri", diagnostics,
                                       &image.resolvedPath, &image.identity);
        }
        asset.images.push_back(std::move(image));
      }
    }

    void parseSamplers(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("samplers");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "samplers", "Expected an array");
        return;
      }

      const QJsonArray samplers = value.toArray();
      asset.samplers.reserve(static_cast<std::size_t>(samplers.size()));
      for (int i = 0; i < samplers.size(); ++i) {
        const std::string path = jsonPath("samplers", static_cast<std::size_t>(i));
        if (!samplers.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = samplers.at(i).toObject();
        Sampler sampler;
        sampler.magFilter = integer(object, "magFilter", path, diagnostics, false);
        sampler.minFilter = integer(object, "minFilter", path, diagnostics, false);
        if (auto wrapS = integer(object, "wrapS", path, diagnostics, false))
          sampler.wrapS = *wrapS;
        if (auto wrapT = integer(object, "wrapT", path, diagnostics, false))
          sampler.wrapT = *wrapT;
        asset.samplers.push_back(sampler);
      }
    }

    void parseTextures(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("textures");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "textures", "Expected an array");
        return;
      }

      const QJsonArray textures = value.toArray();
      asset.textures.reserve(static_cast<std::size_t>(textures.size()));
      for (int i = 0; i < textures.size(); ++i) {
        const std::string path = jsonPath("textures", static_cast<std::size_t>(i));
        if (!textures.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = textures.at(i).toObject();
        Texture texture;
        texture.sampler = unsignedInteger(object, "sampler", path, diagnostics, false);
        texture.source = unsignedInteger(object, "source", path, diagnostics, false);
        asset.textures.push_back(texture);
      }
    }

    void parseMaterials(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("materials");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "materials", "Expected an array");
        return;
      }

      const QJsonArray materials = value.toArray();
      asset.materials.reserve(static_cast<std::size_t>(materials.size()));
      for (int i = 0; i < materials.size(); ++i) {
        const std::string path = jsonPath("materials", static_cast<std::size_t>(i));
        if (!materials.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = materials.at(i).toObject();
        Material material;
        if (auto name = stringProperty(object, "name", path, diagnostics))
          material.name = *name;
        if (auto alphaMode = stringProperty(object, "alphaMode", path, diagnostics))
          material.alphaMode = *alphaMode;
        if (auto alphaCutoff = numberProperty(object, "alphaCutoff", path, diagnostics))
          material.alphaCutoff = *alphaCutoff;
        if (auto doubleSided = boolProperty(object, "doubleSided", path, diagnostics))
          material.doubleSided = *doubleSided;
        if (object.contains("normalTexture"))
          material.unsupportedFeatures.push_back("normalTexture");
        if (object.contains("occlusionTexture"))
          material.unsupportedFeatures.push_back("occlusionTexture");
        if (object.contains("emissiveTexture") || object.contains("emissiveFactor"))
          material.unsupportedFeatures.push_back("emissive");
        if (object.contains("extensions"))
          material.unsupportedFeatures.push_back("material extensions");

        const QJsonValue pbrValue = object.value("pbrMetallicRoughness");
        if (pbrValue.isObject()) {
          const QJsonObject pbr = pbrValue.toObject();
          numberArray(pbr, "baseColorFactor", path + ".pbrMetallicRoughness", diagnostics,
                      &material.baseColorFactor);
          material.baseColorTexture = textureInfoProperty(
            pbr, "baseColorTexture", path + ".pbrMetallicRoughness", diagnostics);
          material.metallicRoughnessTexture = textureInfoProperty(
            pbr, "metallicRoughnessTexture", path + ".pbrMetallicRoughness", diagnostics);
          material.metallicFactor =
            numberProperty(pbr, "metallicFactor", path + ".pbrMetallicRoughness", diagnostics);
          material.roughnessFactor =
            numberProperty(pbr, "roughnessFactor", path + ".pbrMetallicRoughness", diagnostics);
        } else if (!pbrValue.isUndefined()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path + ".pbrMetallicRoughness",
                            "Expected an object");
        }
        asset.materials.push_back(std::move(material));
      }
    }

    void parseMeshes(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("meshes");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "meshes", "Expected an array");
        return;
      }

      const QJsonArray meshes = value.toArray();
      asset.meshes.reserve(static_cast<std::size_t>(meshes.size()));
      for (int i = 0; i < meshes.size(); ++i) {
        const std::string path = jsonPath("meshes", static_cast<std::size_t>(i));
        if (!meshes.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = meshes.at(i).toObject();
        Mesh mesh;
        if (auto name = stringProperty(object, "name", path, diagnostics))
          mesh.name = *name;

        const QJsonValue primitivesValue = object.value("primitives");
        if (!primitivesValue.isArray()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path + ".primitives",
                            "Expected an array");
          asset.meshes.push_back(std::move(mesh));
          continue;
        }

        const QJsonArray primitives = primitivesValue.toArray();
        mesh.primitives.reserve(static_cast<std::size_t>(primitives.size()));
        for (int j = 0; j < primitives.size(); ++j) {
          const std::string primitivePath =
            path + ".primitives[" + std::to_string(static_cast<std::size_t>(j)) + "]";
          if (!primitives.at(j).isObject()) {
            diagnostics.error(DiagnosticCode::InvalidPropertyType, primitivePath,
                              "Expected an object");
            continue;
          }

          const QJsonObject primitiveObject = primitives.at(j).toObject();
          MeshPrimitive primitive;
          primitive.attributes =
            attributeMap(primitiveObject, "attributes", primitivePath, diagnostics);
          primitive.indices =
            unsignedInteger(primitiveObject, "indices", primitivePath, diagnostics, false);
          primitive.material =
            unsignedInteger(primitiveObject, "material", primitivePath, diagnostics, false);
          if (auto mode = integer(primitiveObject, "mode", primitivePath, diagnostics, false))
            primitive.mode = *mode;
          mesh.primitives.push_back(std::move(primitive));
        }
        asset.meshes.push_back(std::move(mesh));
      }
    }

    void parseCameras(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("cameras");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "cameras", "Expected an array");
        return;
      }

      const QJsonArray cameras = value.toArray();
      asset.cameras.reserve(static_cast<std::size_t>(cameras.size()));
      for (int i = 0; i < cameras.size(); ++i) {
        const std::string path = jsonPath("cameras", static_cast<std::size_t>(i));
        if (!cameras.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = cameras.at(i).toObject();
        Camera camera;
        if (auto name = stringProperty(object, "name", path, diagnostics))
          camera.name = *name;
        if (auto typeName = stringProperty(object, "type", path, diagnostics, true)) {
          if (auto type = cameraTypeFromString(*typeName)) {
            camera.type = *type;
          } else {
            diagnostics.error(DiagnosticCode::UnsupportedValue, path + ".type",
                              "Unsupported camera type", *typeName);
          }
        }

        if (camera.type == CameraType::Perspective) {
          const QJsonValue perspectiveValue = object.value("perspective");
          if (!perspectiveValue.isObject()) {
            diagnostics.error(DiagnosticCode::MissingRequiredProperty, path + ".perspective",
                              "perspective camera requires a perspective object");
          } else {
            const QJsonObject perspective = perspectiveValue.toObject();
            const std::string perspectivePath = path + ".perspective";
            camera.perspective.aspectRatio =
              numberProperty(perspective, "aspectRatio", perspectivePath, diagnostics, false);
            if (auto yfov = numberProperty(perspective, "yfov", perspectivePath, diagnostics))
              camera.perspective.yfov = *yfov;
            if (auto znear = numberProperty(perspective, "znear", perspectivePath, diagnostics))
              camera.perspective.znear = *znear;
            camera.perspective.zfar =
              numberProperty(perspective, "zfar", perspectivePath, diagnostics, false);
          }
        } else {
          const QJsonValue orthographicValue = object.value("orthographic");
          if (!orthographicValue.isObject()) {
            diagnostics.error(DiagnosticCode::MissingRequiredProperty, path + ".orthographic",
                              "orthographic camera requires an orthographic object");
          } else {
            const QJsonObject orthographic = orthographicValue.toObject();
            const std::string orthographicPath = path + ".orthographic";
            if (auto xmag = numberProperty(orthographic, "xmag", orthographicPath, diagnostics))
              camera.orthographic.xmag = *xmag;
            if (auto ymag = numberProperty(orthographic, "ymag", orthographicPath, diagnostics))
              camera.orthographic.ymag = *ymag;
            if (auto znear = numberProperty(orthographic, "znear", orthographicPath, diagnostics))
              camera.orthographic.znear = *znear;
            if (auto zfar = numberProperty(orthographic, "zfar", orthographicPath, diagnostics))
              camera.orthographic.zfar = *zfar;
          }
        }

        asset.cameras.push_back(camera);
      }
    }

    void parsePunctualLights(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue extensionsValue = root.value("extensions");
      if (!extensionsValue.isObject())
        return;
      const QJsonValue punctualValue = extensionsValue.toObject().value("KHR_lights_punctual");
      if (!punctualValue.isObject())
        return;
      const QJsonValue lightsValue = punctualValue.toObject().value("lights");
      if (lightsValue.isUndefined())
        return;
      if (!lightsValue.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType,
                          "extensions.KHR_lights_punctual.lights", "Expected an array");
        return;
      }

      const QJsonArray lights = lightsValue.toArray();
      asset.punctualLights.reserve(static_cast<std::size_t>(lights.size()));
      for (int i = 0; i < lights.size(); ++i) {
        const std::string path =
          "extensions.KHR_lights_punctual." + jsonPath("lights", static_cast<std::size_t>(i));
        if (!lights.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = lights.at(i).toObject();
        PunctualLight light;
        if (auto name = stringProperty(object, "name", path, diagnostics))
          light.name = *name;
        if (auto typeName = stringProperty(object, "type", path, diagnostics, true)) {
          if (auto type = punctualLightTypeFromString(*typeName)) {
            light.type = *type;
          } else {
            diagnostics.error(DiagnosticCode::UnsupportedValue, path + ".type",
                              "Unsupported KHR_lights_punctual light type", *typeName);
          }
        }
        numberArray(object, "color", path, diagnostics, &light.color);
        if (auto intensity = numberProperty(object, "intensity", path, diagnostics, false))
          light.intensity = *intensity;
        light.range = numberProperty(object, "range", path, diagnostics, false);

        const QJsonValue spotValue = object.value("spot");
        if (spotValue.isObject()) {
          const QJsonObject spot = spotValue.toObject();
          const std::string spotPath = path + ".spot";
          if (auto inner = numberProperty(spot, "innerConeAngle", spotPath, diagnostics, false))
            light.spot.innerConeAngle = *inner;
          if (auto outer = numberProperty(spot, "outerConeAngle", spotPath, diagnostics, false))
            light.spot.outerConeAngle = *outer;
        } else if (!spotValue.isUndefined()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path + ".spot",
                            "Expected an object");
        }

        asset.punctualLights.push_back(light);
      }
    }

    void parseNodes(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("nodes");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "nodes", "Expected an array");
        return;
      }

      const QJsonArray nodes = value.toArray();
      asset.nodes.reserve(static_cast<std::size_t>(nodes.size()));
      for (int i = 0; i < nodes.size(); ++i) {
        const std::string path = jsonPath("nodes", static_cast<std::size_t>(i));
        if (!nodes.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = nodes.at(i).toObject();
        Node node;
        if (auto name = stringProperty(object, "name", path, diagnostics))
          node.name = *name;
        node.children = indexArray(object, "children", path, diagnostics);
        node.mesh = unsignedInteger(object, "mesh", path, diagnostics, false);
        node.camera = unsignedInteger(object, "camera", path, diagnostics, false);

        const QJsonValue extensionsValue = object.value("extensions");
        if (extensionsValue.isObject()) {
          const QJsonValue punctualValue = extensionsValue.toObject().value("KHR_lights_punctual");
          if (punctualValue.isObject()) {
            node.punctualLight =
              unsignedInteger(punctualValue.toObject(), "light",
                              path + ".extensions.KHR_lights_punctual", diagnostics, false);
          }
        } else if (!extensionsValue.isUndefined()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path + ".extensions",
                            "Expected an object");
        }

        std::array<double, 16> matrix{};
        if (numberArray(object, "matrix", path, diagnostics, &matrix))
          node.matrix = matrix;
        numberArray(object, "translation", path, diagnostics, &node.translation);
        numberArray(object, "rotation", path, diagnostics, &node.rotation);
        numberArray(object, "scale", path, diagnostics, &node.scale);

        asset.nodes.push_back(std::move(node));
      }
    }

    void parseScenes(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue defaultScene = root.value("scene");
      if (!defaultScene.isUndefined()) {
        if (defaultScene.isDouble()) {
          const double number = defaultScene.toDouble();
          const auto integer = static_cast<unsigned long long>(number);
          if (number >= 0.0 && number == static_cast<double>(integer) &&
              integer <= static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
            asset.defaultScene = static_cast<std::size_t>(integer);
          } else {
            diagnostics.error(DiagnosticCode::InvalidPropertyType, "scene",
                              "Expected an unsigned integer");
          }
        } else {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, "scene",
                            "Expected an unsigned integer");
        }
      }

      const QJsonValue value = root.value("scenes");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "scenes", "Expected an array");
        return;
      }

      const QJsonArray scenes = value.toArray();
      asset.scenes.reserve(static_cast<std::size_t>(scenes.size()));
      for (int i = 0; i < scenes.size(); ++i) {
        const std::string path = jsonPath("scenes", static_cast<std::size_t>(i));
        if (!scenes.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = scenes.at(i).toObject();
        Scene scene;
        if (auto name = stringProperty(object, "name", path, diagnostics))
          scene.name = *name;
        scene.nodes = indexArray(object, "nodes", path, diagnostics);
        asset.scenes.push_back(std::move(scene));
      }
    }

    void parseAnimations(const QJsonObject& root, Asset& asset, Diagnostics& diagnostics) {
      const QJsonValue value = root.value("animations");
      if (value.isUndefined())
        return;
      if (!value.isArray()) {
        diagnostics.error(DiagnosticCode::InvalidPropertyType, "animations", "Expected an array");
        return;
      }

      const QJsonArray animations = value.toArray();
      asset.animations.reserve(static_cast<std::size_t>(animations.size()));
      for (int i = 0; i < animations.size(); ++i) {
        const std::string path = jsonPath("animations", static_cast<std::size_t>(i));
        if (!animations.at(i).isObject()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path, "Expected an object");
          continue;
        }

        const QJsonObject object = animations.at(i).toObject();
        Animation animation;
        if (auto name = stringProperty(object, "name", path, diagnostics))
          animation.name = *name;

        const QJsonValue samplersValue = object.value("samplers");
        if (!samplersValue.isArray()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path + ".samplers",
                            "Expected an array");
        } else {
          const QJsonArray samplers = samplersValue.toArray();
          animation.samplers.reserve(static_cast<std::size_t>(samplers.size()));
          for (int j = 0; j < samplers.size(); ++j) {
            const std::string samplerPath =
              path + "." + jsonPath("samplers", static_cast<std::size_t>(j));
            if (!samplers.at(j).isObject()) {
              diagnostics.error(DiagnosticCode::InvalidPropertyType, samplerPath,
                                "Expected an object");
              continue;
            }

            const QJsonObject samplerObject = samplers.at(j).toObject();
            AnimationSampler sampler;
            if (auto input = unsignedInteger(samplerObject, "input", samplerPath, diagnostics))
              sampler.input = *input;
            if (auto output = unsignedInteger(samplerObject, "output", samplerPath, diagnostics))
              sampler.output = *output;
            if (auto interpolation =
                  stringProperty(samplerObject, "interpolation", samplerPath, diagnostics))
              sampler.interpolation = *interpolation;
            animation.samplers.push_back(std::move(sampler));
          }
        }

        const QJsonValue channelsValue = object.value("channels");
        if (!channelsValue.isArray()) {
          diagnostics.error(DiagnosticCode::InvalidPropertyType, path + ".channels",
                            "Expected an array");
        } else {
          const QJsonArray channels = channelsValue.toArray();
          animation.channels.reserve(static_cast<std::size_t>(channels.size()));
          for (int j = 0; j < channels.size(); ++j) {
            const std::string channelPath =
              path + "." + jsonPath("channels", static_cast<std::size_t>(j));
            if (!channels.at(j).isObject()) {
              diagnostics.error(DiagnosticCode::InvalidPropertyType, channelPath,
                                "Expected an object");
              continue;
            }

            const QJsonObject channelObject = channels.at(j).toObject();
            AnimationChannel channel;
            if (auto sampler = unsignedInteger(channelObject, "sampler", channelPath, diagnostics))
              channel.sampler = *sampler;

            const QJsonValue targetValue = channelObject.value("target");
            if (!targetValue.isObject()) {
              diagnostics.error(DiagnosticCode::InvalidPropertyType, channelPath + ".target",
                                "Expected an object");
            } else {
              const QJsonObject targetObject = targetValue.toObject();
              channel.target.node =
                unsignedInteger(targetObject, "node", channelPath + ".target", diagnostics, false);
              if (auto targetPath = stringProperty(targetObject, "path", channelPath + ".target",
                                                   diagnostics, true))
                channel.target.path = *targetPath;
            }
            animation.channels.push_back(std::move(channel));
          }
        }

        asset.animations.push_back(std::move(animation));
      }
    }

    void resolveBufferViewImages(Asset& asset) {
      for (Image& image : asset.images) {
        if (image.bufferView && *image.bufferView < asset.bufferViews.size())
          image.data = bytesFromBufferView(asset, *image.bufferView);
      }
    }

    void validateNodesAndScenes(const Asset& asset, Diagnostics& diagnostics) {
      for (std::size_t i = 0; i < asset.meshes.size(); ++i) {
        for (std::size_t j = 0; j < asset.meshes[i].primitives.size(); ++j) {
          const MeshPrimitive& primitive = asset.meshes[i].primitives[j];
          const std::string path = jsonPath("meshes", i) + ".primitives[" + std::to_string(j) + "]";
          for (const auto& [name, accessor] : primitive.attributes) {
            if (accessor >= asset.accessors.size()) {
              diagnostics.error(DiagnosticCode::InvalidReference, path + ".attributes." + name,
                                "mesh primitive attribute references a missing accessor");
            }
          }
          if (primitive.indices && *primitive.indices >= asset.accessors.size()) {
            diagnostics.error(DiagnosticCode::InvalidReference, path + ".indices",
                              "mesh primitive indices reference a missing accessor");
          }
          if (primitive.material && *primitive.material >= asset.materials.size()) {
            diagnostics.error(DiagnosticCode::InvalidReference, path + ".material",
                              "mesh primitive references a missing material");
          }
        }
      }

      for (std::size_t i = 0; i < asset.nodes.size(); ++i) {
        if (asset.nodes[i].mesh && *asset.nodes[i].mesh >= asset.meshes.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference, jsonPath("nodes", i, "mesh"),
                            "node references a missing mesh");
        }
        if (asset.nodes[i].camera && *asset.nodes[i].camera >= asset.cameras.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference, jsonPath("nodes", i, "camera"),
                            "node references a missing camera");
        }
        if (asset.nodes[i].punctualLight &&
            *asset.nodes[i].punctualLight >= asset.punctualLights.size()) {
          diagnostics.error(DiagnosticCode::InvalidReference,
                            jsonPath("nodes", i, "extensions.KHR_lights_punctual.light"),
                            "node references a missing KHR_lights_punctual light");
        }
        for (const std::size_t child : asset.nodes[i].children) {
          if (child >= asset.nodes.size()) {
            diagnostics.error(DiagnosticCode::InvalidReference, jsonPath("nodes", i, "children"),
                              "node references a missing child node");
          }
        }
      }

      for (std::size_t i = 0; i < asset.scenes.size(); ++i) {
        for (const std::size_t node : asset.scenes[i].nodes) {
          if (node >= asset.nodes.size()) {
            diagnostics.error(DiagnosticCode::InvalidReference, jsonPath("scenes", i, "nodes"),
                              "scene references a missing node");
          }
        }
      }

      if (asset.defaultScene && *asset.defaultScene >= asset.scenes.size()) {
        diagnostics.error(DiagnosticCode::InvalidReference, "scene",
                          "default scene references a missing scene");
      }

      for (std::size_t i = 0; i < asset.animations.size(); ++i) {
        const Animation& animation = asset.animations[i];
        for (std::size_t j = 0; j < animation.samplers.size(); ++j) {
          const AnimationSampler& sampler = animation.samplers[j];
          const std::string path = jsonPath("animations", i) + "." + jsonPath("samplers", j);
          if (sampler.input >= asset.accessors.size()) {
            diagnostics.error(DiagnosticCode::InvalidReference, path + ".input",
                              "animation sampler references a missing input accessor");
          }
          if (sampler.output >= asset.accessors.size()) {
            diagnostics.error(DiagnosticCode::InvalidReference, path + ".output",
                              "animation sampler references a missing output accessor");
          }
        }
        for (std::size_t j = 0; j < animation.channels.size(); ++j) {
          const AnimationChannel& channel = animation.channels[j];
          const std::string path = jsonPath("animations", i) + "." + jsonPath("channels", j);
          if (channel.sampler >= animation.samplers.size()) {
            diagnostics.error(DiagnosticCode::InvalidReference, path + ".sampler",
                              "animation channel references a missing sampler");
          }
          if (channel.target.node && *channel.target.node >= asset.nodes.size()) {
            diagnostics.error(DiagnosticCode::InvalidReference, path + ".target.node",
                              "animation channel references a missing node");
          }
        }
      }
    }

    ReadResult parseDocument(const QByteArray& jsonBytes, const fs::path& currentFile,
                             const AssetResolver& resolver,
                             const std::vector<std::uint8_t>& glbBinaryChunk = {}) {
      ReadResult result;
      QJsonParseError error;
      const QJsonDocument document = QJsonDocument::fromJson(jsonBytes, &error);
      if (error.error != QJsonParseError::NoError) {
        result.diagnostics.error(DiagnosticCode::InvalidJson, "json",
                                 error.errorString().toStdString());
        return result;
      }
      if (!document.isObject()) {
        result.diagnostics.error(DiagnosticCode::InvalidJson, "json", "Root must be a JSON object");
        return result;
      }

      const QJsonObject root = document.object();
      Asset asset;
      const QJsonValue assetValue = root.value("asset");
      if (!assetValue.isObject()) {
        result.diagnostics.error(DiagnosticCode::MissingRequiredProperty, "asset",
                                 "glTF asset metadata object is required");
      } else {
        const QJsonObject assetObject = assetValue.toObject();
        if (auto version =
              stringProperty(assetObject, "version", "asset", result.diagnostics, true))
          asset.version = *version;
        if (asset.version.rfind("2.", 0) != 0) {
          result.diagnostics.error(DiagnosticCode::UnsupportedVersion, "asset.version",
                                   "Only glTF 2.x assets are supported", asset.version);
        }
        if (auto generator = stringProperty(assetObject, "generator", "asset", result.diagnostics))
          asset.generator = *generator;
      }

      parseBuffers(root, asset, currentFile, resolver, result.diagnostics, glbBinaryChunk);
      parseBufferViews(root, asset, result.diagnostics);
      parseAccessors(root, asset, result.diagnostics);
      parseImages(root, asset, currentFile, resolver, result.diagnostics);
      parseSamplers(root, asset, result.diagnostics);
      parseTextures(root, asset, result.diagnostics);
      parseMaterials(root, asset, result.diagnostics);
      parseMeshes(root, asset, result.diagnostics);
      parseCameras(root, asset, result.diagnostics);
      parsePunctualLights(root, asset, result.diagnostics);
      parseNodes(root, asset, result.diagnostics);
      parseScenes(root, asset, result.diagnostics);
      parseAnimations(root, asset, result.diagnostics);
      validateBufferViews(asset, result.diagnostics);
      validateAccessors(asset, result.diagnostics);
      validateImages(asset, result.diagnostics);
      validateTexturesAndMaterials(asset, result.diagnostics);
      validateNodesAndScenes(asset, result.diagnostics);
      resolveBufferViewImages(asset);

      if (!result.diagnostics.hasErrors())
        result.asset = std::move(asset);
      return result;
    }
  }

  ReadResult Reader::readFile(const fs::path& path, AssetResolver resolver) {
    ReadResult ioResult;
    const std::vector<std::uint8_t> bytes = readAllBytes(path, ioResult.diagnostics);
    if (ioResult.diagnostics.hasErrors())
      return ioResult;

    const std::string extension = path.extension().string();
    if (extension == ".glb" || extension == ".GLB")
      return readGlb(bytes, path, std::move(resolver));
    return readJson(bytesToString(bytes), path, std::move(resolver));
  }

  ReadResult Reader::readJson(const std::string& json, const fs::path& currentFile,
                              AssetResolver resolver) {
    return parseDocument(QByteArray(json.data(), static_cast<int>(json.size())), currentFile,
                         resolver);
  }

  ReadResult Reader::readGlb(const std::vector<std::uint8_t>& bytes, const fs::path& currentFile,
                             AssetResolver resolver) {
    ReadResult result;
    if (bytes.size() < 12) {
      result.diagnostics.error(DiagnosticCode::InvalidGlb, "glb", "GLB header is truncated");
      return result;
    }

    const std::uint32_t magic = readUint32Le(bytes, 0);
    const std::uint32_t version = readUint32Le(bytes, 4);
    const std::uint32_t length = readUint32Le(bytes, 8);
    if (magic != glbMagic) {
      result.diagnostics.error(DiagnosticCode::InvalidGlb, "glb.magic", "Invalid GLB magic");
      return result;
    }
    if (version != 2) {
      result.diagnostics.error(DiagnosticCode::UnsupportedVersion, "glb.version",
                               "Only GLB version 2 is supported");
      return result;
    }
    if (length != bytes.size()) {
      result.diagnostics.error(DiagnosticCode::InvalidGlb, "glb.length",
                               "GLB length field does not match file size");
      return result;
    }

    std::optional<QByteArray> jsonChunk;
    std::vector<std::uint8_t> binaryChunk;
    std::size_t offset = 12;
    while (offset < bytes.size()) {
      if (bytes.size() - offset < 8) {
        result.diagnostics.error(DiagnosticCode::InvalidGlb, "glb.chunks",
                                 "Chunk header is truncated");
        return result;
      }
      const std::uint32_t chunkLength = readUint32Le(bytes, offset);
      const std::uint32_t chunkType = readUint32Le(bytes, offset + 4);
      offset += 8;
      if (chunkLength > bytes.size() - offset) {
        result.diagnostics.error(DiagnosticCode::InvalidGlb, "glb.chunks",
                                 "Chunk length exceeds GLB size");
        return result;
      }

      if (chunkType == glbJsonChunk) {
        if (jsonChunk) {
          result.diagnostics.error(DiagnosticCode::InvalidGlb, "glb.chunks",
                                   "GLB contains more than one JSON chunk");
          return result;
        }
        jsonChunk = QByteArray(reinterpret_cast<const char*>(bytes.data() + offset),
                               static_cast<int>(chunkLength));
      } else if (chunkType == glbBinaryChunk) {
        if (!binaryChunk.empty()) {
          result.diagnostics.error(DiagnosticCode::InvalidGlb, "glb.chunks",
                                   "GLB contains more than one BIN chunk");
          return result;
        }
        binaryChunk.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                           bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
      }
      offset += chunkLength;
    }

    if (!jsonChunk) {
      result.diagnostics.error(DiagnosticCode::InvalidGlb, "glb.chunks",
                               "GLB JSON chunk is missing");
      return result;
    }

    return parseDocument(*jsonChunk, currentFile, resolver, binaryChunk);
  }

}
