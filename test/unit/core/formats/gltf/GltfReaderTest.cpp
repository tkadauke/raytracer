#include <gtest/gtest.h>

#include "core/formats/gltf/GltfReader.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

namespace GltfReaderTest {
  using core::gltf::AccessorType;
  using core::gltf::ComponentType;
  using core::gltf::DiagnosticCode;
  using core::gltf::Reader;

  bool hasDiagnostic(const core::gltf::ReadResult& result, DiagnosticCode code) {
    for (const auto& diagnostic : result.diagnostics.entries()) {
      if (diagnostic.code == code)
        return true;
    }
    return false;
  }

  void appendUint32(vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
  }

  vector<uint8_t> makeGlb(string json, vector<uint8_t> binary) {
    while ((json.size() % 4) != 0)
      json.push_back(' ');
    while ((binary.size() % 4) != 0)
      binary.push_back(0);

    vector<uint8_t> bytes;
    appendUint32(bytes, 0x46546c67u);
    appendUint32(bytes, 2);
    appendUint32(bytes, static_cast<uint32_t>(12 + 8 + json.size() + 8 + binary.size()));
    appendUint32(bytes, static_cast<uint32_t>(json.size()));
    appendUint32(bytes, 0x4e4f534au);
    bytes.insert(bytes.end(), json.begin(), json.end());
    appendUint32(bytes, static_cast<uint32_t>(binary.size()));
    appendUint32(bytes, 0x004e4942u);
    bytes.insert(bytes.end(), binary.begin(), binary.end());
    return bytes;
  }

  TEST(GltfReader, ParsesGltfJsonAndResolvesExternalAssetsBesideFile) {
    const fs::path fixture = "test/fixtures/gltf/external_triangle.gltf";

    const core::gltf::ReadResult result = Reader::readFile(fixture);

    ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
                                   ? ""
                                   : result.diagnostics.entries().front().toString());
    ASSERT_TRUE(result.asset);
    EXPECT_EQ("2.0", result.asset->version);
    ASSERT_EQ(1u, result.asset->buffers.size());
    EXPECT_EQ(12u, result.asset->buffers[0].byteLength);
    ASSERT_GE(result.asset->buffers[0].data.size(), result.asset->buffers[0].byteLength);
    EXPECT_EQ("abcdefghijkl", string(result.asset->buffers[0].data.begin(),
                                     result.asset->buffers[0].data.begin() + 12));
    ASSERT_EQ(1u, result.asset->bufferViews.size());
    EXPECT_EQ(12u, result.asset->bufferViews[0].byteStride.value());
    ASSERT_EQ(1u, result.asset->accessors.size());
    EXPECT_EQ(ComponentType::Float32, result.asset->accessors[0].componentType);
    EXPECT_EQ(AccessorType::Vec3, result.asset->accessors[0].type);
    ASSERT_EQ(1u, result.asset->images.size());
    EXPECT_EQ("image/png", result.asset->images[0].mimeType);
    EXPECT_GE(result.asset->images[0].data.size(), 8u);
  }

  TEST(GltfReader, DecodesEmbeddedBufferAndImageDataUris) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [
        {"uri": "data:application/octet-stream;base64,AQIDBA==", "byteLength": 4}
      ],
      "images": [
        {"uri": "data:image/png;base64,cG5n", "mimeType": "image/png"}
      ]
    })JSON";

    const core::gltf::ReadResult result = Reader::readJson(json);

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.asset);
    ASSERT_EQ(1u, result.asset->buffers.size());
    EXPECT_EQ((vector<uint8_t>{1, 2, 3, 4}), result.asset->buffers[0].data);
    ASSERT_EQ(1u, result.asset->images.size());
    EXPECT_EQ((vector<uint8_t>{'p', 'n', 'g'}), result.asset->images[0].data);
  }

  TEST(GltfReader, ParsesPbrMaterialTextureAndSamplerMetadata) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "images": [{"uri": "data:image/png;base64,cG5n", "mimeType": "image/png"}],
      "samplers": [{"magFilter": 9728, "minFilter": 9987, "wrapS": 33071, "wrapT": 10497}],
      "textures": [{"sampler": 0, "source": 0}],
      "materials": [{
        "name": "Paint",
        "pbrMetallicRoughness": {
          "baseColorFactor": [0.25, 0.5, 0.75, 0.8],
          "baseColorTexture": {"index": 0, "texCoord": 0},
          "metallicFactor": 0.2,
          "roughnessFactor": 0.7,
          "metallicRoughnessTexture": {"index": 0}
        },
        "alphaMode": "BLEND",
        "alphaCutoff": 0.4,
        "doubleSided": true,
        "normalTexture": {"index": 0},
        "extensions": {"KHR_materials_unlit": {}}
      }]
    })JSON";

    const core::gltf::ReadResult result = Reader::readJson(json);

    ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
                                   ? ""
                                   : result.diagnostics.entries().front().toString());
    ASSERT_TRUE(result.asset);
    ASSERT_EQ(1u, result.asset->samplers.size());
    EXPECT_EQ(9728, result.asset->samplers[0].magFilter.value());
    EXPECT_EQ(9987, result.asset->samplers[0].minFilter.value());
    EXPECT_EQ(33071, result.asset->samplers[0].wrapS);
    ASSERT_EQ(1u, result.asset->textures.size());
    EXPECT_EQ(0u, result.asset->textures[0].source.value());
    EXPECT_EQ(0u, result.asset->textures[0].sampler.value());
    ASSERT_EQ(1u, result.asset->materials.size());
    const auto& material = result.asset->materials[0];
    EXPECT_EQ("Paint", material.name);
    EXPECT_EQ(0.25, material.baseColorFactor[0]);
    ASSERT_TRUE(material.baseColorTexture.has_value());
    EXPECT_EQ(0u, material.baseColorTexture->index);
    EXPECT_EQ(0, material.baseColorTexture->texCoord);
    EXPECT_EQ(0.2, material.metallicFactor.value());
    EXPECT_EQ(0.7, material.roughnessFactor.value());
    ASSERT_TRUE(material.metallicRoughnessTexture.has_value());
    EXPECT_EQ("BLEND", material.alphaMode);
    EXPECT_EQ(0.4, material.alphaCutoff);
    EXPECT_TRUE(material.doubleSided);
    EXPECT_FALSE(material.unsupportedFeatures.empty());
  }

  TEST(GltfReader, ParsesScenesNodesNamesAndTransforms) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "scene": 1,
      "scenes": [
        {"name": "Left scene", "nodes": [0]},
        {"name": "Right scene", "nodes": [2]}
      ],
      "nodes": [
        {"name": "Root", "translation": [1, 2, 3], "children": [1]},
        {"name": "Leaf", "scale": [2, 3, 4]},
        {"name": "Matrix node", "matrix": [
          1, 0, 0, 0,
          0, 1, 0, 0,
          0, 0, 1, 0,
          5, 6, 7, 1
        ]}
      ]
    })JSON";

    const core::gltf::ReadResult result = Reader::readJson(json);

    ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
                                   ? ""
                                   : result.diagnostics.entries().front().toString());
    ASSERT_TRUE(result.asset);
    ASSERT_EQ(2u, result.asset->scenes.size());
    EXPECT_EQ(1u, result.asset->defaultScene.value());
    EXPECT_EQ("Left scene", result.asset->scenes[0].name);
    EXPECT_EQ((vector<size_t>{0}), result.asset->scenes[0].nodes);
    EXPECT_EQ("Right scene", result.asset->scenes[1].name);
    ASSERT_EQ(3u, result.asset->nodes.size());
    EXPECT_EQ("Root", result.asset->nodes[0].name);
    EXPECT_EQ((vector<size_t>{1}), result.asset->nodes[0].children);
    EXPECT_EQ(1.0, result.asset->nodes[0].translation[0]);
    EXPECT_EQ(3.0, result.asset->nodes[0].translation[2]);
    EXPECT_EQ("Leaf", result.asset->nodes[1].name);
    EXPECT_EQ(4.0, result.asset->nodes[1].scale[2]);
    ASSERT_TRUE(result.asset->nodes[2].matrix.has_value());
    EXPECT_EQ(5.0, (*result.asset->nodes[2].matrix)[12]);
  }

  TEST(GltfReader, ParsesAnimationSamplersAndChannelsFromFixture) {
    const fs::path fixture = "test/fixtures/gltf/animated_node.gltf";

    const core::gltf::ReadResult result = Reader::readFile(fixture);
    ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
                                   ? ""
                                   : result.diagnostics.entries().front().toString());
    ASSERT_TRUE(result.asset);
    ASSERT_EQ(1u, result.asset->animations.size());
    const auto& animation = result.asset->animations.front();
    EXPECT_EQ("Move", animation.name);
    ASSERT_EQ(2u, animation.samplers.size());
    EXPECT_EQ(0u, animation.samplers[0].input);
    EXPECT_EQ(1u, animation.samplers[0].output);
    EXPECT_EQ("LINEAR", animation.samplers[0].interpolation);
    ASSERT_EQ(2u, animation.channels.size());
    EXPECT_EQ(0u, animation.channels[0].sampler);
    ASSERT_TRUE(animation.channels[0].target.node.has_value());
    EXPECT_EQ(0u, *animation.channels[0].target.node);
    EXPECT_EQ("translation", animation.channels[0].target.path);
    EXPECT_EQ("weights", animation.channels[1].target.path);
  }

  TEST(GltfReader, ParsesCamerasAndPunctualLightExtension) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "extensions": {
        "KHR_lights_punctual": {
          "lights": [
            {"name": "Sun", "type": "directional", "color": [1.0, 0.8, 0.6], "intensity": 2.5},
            {"name": "Lamp", "type": "point", "range": 12.0}
          ]
        }
      },
      "cameras": [
        {"name": "Hero", "type": "perspective",
         "perspective": {"yfov": 0.7, "aspectRatio": 1.5, "znear": 0.1, "zfar": 100.0}},
        {"name": "Plan", "type": "orthographic",
         "orthographic": {"xmag": 8.0, "ymag": 4.0, "znear": 0.1, "zfar": 50.0}}
      ],
      "nodes": [
        {"name": "Camera Node", "camera": 0},
        {"name": "Light Node", "extensions": {"KHR_lights_punctual": {"light": 1}}}
      ]
    })JSON";

    const core::gltf::ReadResult result = Reader::readJson(json);

    ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
                                   ? ""
                                   : result.diagnostics.entries().front().toString());
    ASSERT_TRUE(result.asset);
    ASSERT_EQ(2u, result.asset->cameras.size());
    EXPECT_EQ("Hero", result.asset->cameras[0].name);
    EXPECT_EQ(core::gltf::CameraType::Perspective, result.asset->cameras[0].type);
    EXPECT_EQ(0.7, result.asset->cameras[0].perspective.yfov);
    EXPECT_EQ(1.5, result.asset->cameras[0].perspective.aspectRatio.value());
    EXPECT_EQ(core::gltf::CameraType::Orthographic, result.asset->cameras[1].type);
    EXPECT_EQ(8.0, result.asset->cameras[1].orthographic.xmag);
    ASSERT_EQ(2u, result.asset->punctualLights.size());
    EXPECT_EQ("Sun", result.asset->punctualLights[0].name);
    EXPECT_EQ(core::gltf::PunctualLightType::Directional, result.asset->punctualLights[0].type);
    EXPECT_EQ(0.8, result.asset->punctualLights[0].color[1]);
    EXPECT_EQ(core::gltf::PunctualLightType::Point, result.asset->punctualLights[1].type);
    EXPECT_EQ(12.0, result.asset->punctualLights[1].range.value());
    EXPECT_EQ(0u, result.asset->nodes[0].camera.value());
    EXPECT_EQ(1u, result.asset->nodes[1].punctualLight.value());
  }

  TEST(GltfReader, ParsesGlbJsonAndBinaryChunks) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{"byteLength": 4}],
      "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 4}],
      "accessors": [{"bufferView": 0, "componentType": 5121, "count": 4, "type": "SCALAR"}],
      "images": [{"bufferView": 0, "mimeType": "image/png"}]
    })JSON";
    const vector<uint8_t> glb = makeGlb(json, {9, 8, 7, 6});

    const core::gltf::ReadResult result = Reader::readGlb(glb);

    ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
                                   ? ""
                                   : result.diagnostics.entries().front().toString());
    ASSERT_TRUE(result.asset);
    ASSERT_EQ(1u, result.asset->buffers.size());
    EXPECT_EQ((vector<uint8_t>{9, 8, 7, 6}), result.asset->buffers[0].data);
    ASSERT_EQ(1u, result.asset->images.size());
    EXPECT_EQ((vector<uint8_t>{9, 8, 7, 6}), result.asset->images[0].data);
  }

  TEST(GltfReader, RejectsMalformedJsonWithStructuredDiagnostic) {
    const core::gltf::ReadResult result = Reader::readJson("{");

    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(hasDiagnostic(result, DiagnosticCode::InvalidJson));
    ASSERT_FALSE(result.diagnostics.entries().empty());
    EXPECT_NE(string::npos, result.diagnostics.entries().front().toString().find("glTF error"));
  }

  TEST(GltfReader, RejectsInvalidAccessorComponentTypesAndStrides) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAA", "byteLength": 12}],
      "bufferViews": [{"buffer": 0, "byteLength": 12, "byteStride": 2}],
      "accessors": [{"bufferView": 0, "componentType": 5130, "count": 2, "type": "VEC3"}]
    })JSON";

    const core::gltf::ReadResult result = Reader::readJson(json);

    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(hasDiagnostic(result, DiagnosticCode::InvalidAccessor));
  }

  TEST(GltfReader, RejectsAccessorRangesOutsideBufferView) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{"uri": "data:application/octet-stream;base64,AAAAAAAA", "byteLength": 6}],
      "bufferViews": [{"buffer": 0, "byteLength": 6}],
      "accessors": [{"bufferView": 0, "componentType": 5126, "count": 2, "type": "VEC3"}]
    })JSON";

    const core::gltf::ReadResult result = Reader::readJson(json);

    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(hasDiagnostic(result, DiagnosticCode::InvalidAccessor));
  }

  TEST(GltfReader, ReportsMissingExternalBuffersThroughAssetResolverDiagnostics) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{"uri": "missing.bin", "byteLength": 4}]
    })JSON";

    const core::gltf::ReadResult result =
      Reader::readJson(json, "test/fixtures/gltf/missing_source.gltf");

    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(hasDiagnostic(result, DiagnosticCode::AssetResolutionFailed));
    ASSERT_FALSE(result.diagnostics.entries().empty());
    EXPECT_EQ("missing.bin", result.diagnostics.entries().front().reference);
  }

  TEST(GltfReader, RejectsMissingSceneAndNodeReferences) {
    const string json = R"JSON({
      "asset": {"version": "2.0"},
      "scene": 3,
      "scenes": [{"nodes": [2]}],
      "nodes": [{"children": [1]}]
    })JSON";

    const core::gltf::ReadResult result = Reader::readJson(json);

    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(hasDiagnostic(result, DiagnosticCode::InvalidReference));
  }

}
