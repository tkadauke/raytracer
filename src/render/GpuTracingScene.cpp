#include "render/GpuTracingScene.h"

#include "render/IntersectionSceneCompiler.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/lights/PointLight.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Scene.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/Texture.h"
#include "render/textures/mappings/PlanarMapping2D.h"
#include "render/textures/mappings/UVMapping2D.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

using namespace render;

namespace {
  template<typename T>
  constexpr bool isKernelRecord() {
    return std::is_standard_layout_v<T> && alignof(T) == 16 && sizeof(T) % 16 == 0;
  }

  static_assert(isKernelRecord<GpuTracingMaterialRecord>());
  static_assert(isKernelRecord<GpuTracingTextureRecord>());
  static_assert(isKernelRecord<GpuTracingLightRecord>());
  static_assert(isKernelRecord<GpuTracingEnvironmentRecord>());
  static_assert(isKernelRecord<GpuTracingDebugIdRecord>());
  static_assert(isKernelRecord<GpuTracingShadingRecord>());
  static_assert(isKernelRecord<GpuDiffusePathStateRecord>());
  static_assert(isKernelRecord<GpuDiffusePathStepRecord>());

  template<typename Record>
  GpuTracingSceneSectionLayout sectionLayout(GpuTracingSceneSectionKind kind,
                                             std::uint32_t recordCount, std::uint32_t byteOffset) {
    const std::uint32_t recordSize = static_cast<std::uint32_t>(sizeof(Record));
    return GpuTracingSceneSectionLayout{kind,
                                        gpuTracingSceneLayoutVersion,
                                        recordCount,
                                        recordSize,
                                        static_cast<std::uint32_t>(alignof(Record)),
                                        byteOffset,
                                        recordCount * recordSize};
  }

  std::uint32_t byteCountFor(std::size_t value) {
    return value > std::numeric_limits<std::uint32_t>::max()
             ? std::numeric_limits<std::uint32_t>::max()
             : static_cast<std::uint32_t>(value);
  }

  template<typename Record>
  void appendRecordBytes(std::vector<std::uint8_t>& target, const std::vector<Record>& records) {
    if (records.empty()) {
      return;
    }

    const std::size_t offset = target.size();
    const std::size_t bytes = records.size() * sizeof(Record);
    target.resize(offset + bytes);
    std::memcpy(target.data() + offset, records.data(), bytes);
  }

  void appendGeometryBytes(std::vector<std::uint8_t>& target,
                           const GpuIntersectionSceneBuffers& geometry) {
    appendRecordBytes(target, geometry.bvh);
    appendRecordBytes(target, geometry.primitives);
    appendRecordBytes(target, geometry.triangles);
    appendRecordBytes(target, geometry.spheres);
    appendRecordBytes(target, geometry.planes);
    appendRecordBytes(target, geometry.rectangles);
    appendRecordBytes(target, geometry.disks);
    appendRecordBytes(target, geometry.openCylinders);
    appendRecordBytes(target, geometry.tori);
    appendRecordBytes(target, geometry.transforms);
  }

  void setUnsupportedReason(std::string* unsupportedReason, const char* reason) {
    if (unsupportedReason) {
      *unsupportedReason = reason;
    }
  }

  template<typename Unsupported>
  std::vector<GpuTracingUnsupportedReasonCount>
  unsupportedReasonCountsFor(const std::vector<Unsupported>& unsupportedItems) {
    std::vector<GpuTracingUnsupportedReasonCount> result;
    for (const Unsupported& unsupported : unsupportedItems) {
      const auto existing =
        std::find_if(result.begin(), result.end(),
                     [&unsupported](const GpuTracingUnsupportedReasonCount& count) {
                       return count.reason == unsupported.reason;
                     });
      if (existing != result.end()) {
        ++existing->count;
      } else {
        result.push_back(GpuTracingUnsupportedReasonCount{unsupported.reason, 1});
      }
    }
    return result;
  }

  void insertReasonCounts(std::map<std::string, std::uint64_t>& target,
                          const std::vector<GpuTracingUnsupportedReasonCount>& counts) {
    for (const GpuTracingUnsupportedReasonCount& count : counts) {
      const std::string label = count.reason.empty() ? "unknown" : count.reason;
      target[label] += count.count;
    }
  }

  bool rejectNormalTexture(const MatteMaterial& material, std::string* unsupportedReason) {
    if (material.normalTexture()) {
      setUnsupportedReason(unsupportedReason,
                           "normal textures are not supported by GPU tracing scene compiler");
      return true;
    }
    return false;
  }

  void packLocalPhongParameters(const PhongMaterial& material, GpuTracingMaterialRecord& record) {
    record.parameters = {static_cast<float>(material.ambientCoefficient()),
                         static_cast<float>(material.diffuseCoefficient()),
                         static_cast<float>(material.specularCoefficient()),
                         static_cast<float>(material.exponent())};
  }

  void packMirrorContinuationParameters(const ReflectiveMaterial& material,
                                        GpuTracingMaterialRecord& record) {
    record.continuationParameters = {static_cast<float>(material.reflectionColor().r()),
                                     static_cast<float>(material.reflectionColor().g()),
                                     static_cast<float>(material.reflectionColor().b()),
                                     static_cast<float>(material.reflectionCoefficient())};
  }

  std::optional<std::uint32_t> mappingFlagsFor(const TextureMapping2D* mapping,
                                               std::string* unsupportedReason) {
    if (const auto* uvMapping = dynamic_cast<const UVMapping2D*>(mapping)) {
      (void)uvMapping;
      return static_cast<std::uint32_t>(GpuTracingTextureMappingKind::UV);
    }
    if (dynamic_cast<const PlanarMapping2D*>(mapping)) {
      return static_cast<std::uint32_t>(GpuTracingTextureMappingKind::Planar);
    }

    setUnsupportedReason(unsupportedReason,
                         "texture mapping is not supported by GPU tracing scene compiler");
    return std::nullopt;
  }

  void packTextureMappingParameters(const TextureMapping2D* mapping,
                                    GpuTracingTextureRecord& record) {
    if (const auto* uvMapping = dynamic_cast<const UVMapping2D*>(mapping)) {
      record.parameters[0] = static_cast<float>(uvMapping->uScale());
      record.parameters[1] = static_cast<float>(uvMapping->vScale());
      return;
    }
    record.parameters[0] = 1.0f;
    record.parameters[1] = 1.0f;
  }

  class GpuTracingMaterialResourceContext {
  public:
    virtual ~GpuTracingMaterialResourceContext() = default;

    virtual std::uint32_t textureIdFor(const std::shared_ptr<Texturec>& texture) = 0;
    virtual std::uint32_t constantColorTexture(const Colord& color) = 0;
  };

  class FixedGpuTracingMaterialResourceContext final : public GpuTracingMaterialResourceContext {
  public:
    FixedGpuTracingMaterialResourceContext(std::uint32_t albedoTexture,
                                           std::uint32_t emissionTexture)
        : m_albedoTexture(albedoTexture),
          m_emissionTexture(emissionTexture) {
    }

    std::uint32_t textureIdFor(const std::shared_ptr<Texturec>&) override {
      return m_albedoTexture;
    }

    std::uint32_t constantColorTexture(const Colord&) override {
      return m_emissionTexture;
    }

  private:
    std::uint32_t m_albedoTexture;
    std::uint32_t m_emissionTexture;
  };

  class FunctionGpuTracingMaterialResourceContext final : public GpuTracingMaterialResourceContext {
  public:
    using TextureIdFor = std::function<std::uint32_t(const std::shared_ptr<Texturec>& texture)>;
    using AppendConstantColorTexture = std::function<std::uint32_t(const Colord& color)>;

    FunctionGpuTracingMaterialResourceContext(TextureIdFor textureIdFor,
                                              AppendConstantColorTexture appendConstantColorTexture)
        : m_textureIdFor(std::move(textureIdFor)),
          m_appendConstantColorTexture(std::move(appendConstantColorTexture)) {
    }

    std::uint32_t textureIdFor(const std::shared_ptr<Texturec>& texture) override {
      return m_textureIdFor(texture);
    }

    std::uint32_t constantColorTexture(const Colord& color) override {
      return m_appendConstantColorTexture(color);
    }

  private:
    TextureIdFor m_textureIdFor;
    AppendConstantColorTexture m_appendConstantColorTexture;
  };

  class GpuTracingTextureResourceContext {
  public:
    virtual ~GpuTracingTextureResourceContext() = default;

    virtual std::uint32_t textureIdFor(const std::shared_ptr<Texturec>& texture) = 0;
    virtual std::uint32_t constantColorTexture(const Colord& color) = 0;
  };

  class FunctionGpuTracingTextureResourceContext final : public GpuTracingTextureResourceContext {
  public:
    using TextureIdFor = std::function<std::uint32_t(const std::shared_ptr<Texturec>& texture)>;
    using AppendConstantColorTexture = std::function<std::uint32_t(const Colord& color)>;

    FunctionGpuTracingTextureResourceContext(TextureIdFor textureIdFor,
                                             AppendConstantColorTexture appendConstantColorTexture)
        : m_textureIdFor(std::move(textureIdFor)),
          m_appendConstantColorTexture(std::move(appendConstantColorTexture)) {
    }

    std::uint32_t textureIdFor(const std::shared_ptr<Texturec>& texture) override {
      return m_textureIdFor(texture);
    }

    std::uint32_t constantColorTexture(const Colord& color) override {
      return m_appendConstantColorTexture(color);
    }

  private:
    TextureIdFor m_textureIdFor;
    AppendConstantColorTexture m_appendConstantColorTexture;
  };

  class GpuTracingMaterialModel {
  public:
    virtual ~GpuTracingMaterialModel() = default;

    virtual std::optional<GpuTracingMaterialRecord>
    record(GpuTracingMaterialResourceContext& resources, std::string* unsupportedReason) const = 0;
  };

  class GpuTracingMatteMaterialModel final : public GpuTracingMaterialModel {
  public:
    explicit GpuTracingMatteMaterialModel(const MatteMaterial& material)
        : m_material(material) {
    }

    std::optional<GpuTracingMaterialRecord> record(GpuTracingMaterialResourceContext& resources,
                                                   std::string* unsupportedReason) const override {
      if (rejectNormalTexture(m_material, unsupportedReason)) {
        return std::nullopt;
      }

      GpuTracingMaterialRecord record;
      record.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte);
      record.albedoTexture = resources.textureIdFor(m_material.diffuseTexture());
      record.parameters = {static_cast<float>(m_material.ambientCoefficient()),
                           static_cast<float>(m_material.diffuseCoefficient()), 0.0f, 0.0f};
      return record;
    }

  private:
    const MatteMaterial& m_material;
  };

  class GpuTracingPhongMaterialModel final : public GpuTracingMaterialModel {
  public:
    explicit GpuTracingPhongMaterialModel(const PhongMaterial& material)
        : m_material(material) {
    }

    std::optional<GpuTracingMaterialRecord> record(GpuTracingMaterialResourceContext& resources,
                                                   std::string* unsupportedReason) const override {
      if (rejectNormalTexture(m_material, unsupportedReason)) {
        return std::nullopt;
      }

      GpuTracingMaterialRecord record;
      record.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Phong);
      record.albedoTexture = resources.textureIdFor(m_material.diffuseTexture());
      packLocalPhongParameters(m_material, record);
      return record;
    }

  private:
    const PhongMaterial& m_material;
  };

  class GpuTracingReflectiveMaterialModel final : public GpuTracingMaterialModel {
  public:
    explicit GpuTracingReflectiveMaterialModel(const ReflectiveMaterial& material)
        : m_material(material) {
    }

    std::optional<GpuTracingMaterialRecord> record(GpuTracingMaterialResourceContext& resources,
                                                   std::string* unsupportedReason) const override {
      if (rejectNormalTexture(m_material, unsupportedReason)) {
        return std::nullopt;
      }

      GpuTracingMaterialRecord record;
      record.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Reflective);
      record.albedoTexture = resources.textureIdFor(m_material.diffuseTexture());
      packLocalPhongParameters(m_material, record);
      packMirrorContinuationParameters(m_material, record);
      return record;
    }

  private:
    const ReflectiveMaterial& m_material;
  };

  class GpuTracingEmissiveMaterialModel final : public GpuTracingMaterialModel {
  public:
    explicit GpuTracingEmissiveMaterialModel(const EmissiveMaterial& material)
        : m_material(material) {
    }

    std::optional<GpuTracingMaterialRecord> record(GpuTracingMaterialResourceContext& resources,
                                                   std::string*) const override {
      GpuTracingMaterialRecord record;
      record.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Emissive);
      record.emissionTexture = resources.constantColorTexture(m_material.radiance());
      return record;
    }

  private:
    const EmissiveMaterial& m_material;
  };

  class GpuTracingMaterialLoweringVisitor final : public MaterialVisitor {
  public:
    void visit(const Material&) override {
      m_unsupportedReason = "material type is not supported by GPU tracing scene compiler";
      m_model.reset();
    }

    void visit(const MatteMaterial& material) override {
      m_model = std::make_unique<GpuTracingMatteMaterialModel>(material);
    }

    void visit(const PhongMaterial& material) override {
      m_model = std::make_unique<GpuTracingPhongMaterialModel>(material);
    }

    void visit(const ReflectiveMaterial& material) override {
      m_model = std::make_unique<GpuTracingReflectiveMaterialModel>(material);
    }

    void visit(const TransparentMaterial&) override {
      m_unsupportedReason = "transparent/refraction materials are not supported by GPU Whitted v1";
      m_model.reset();
    }

    void visit(const EmissiveMaterial& material) override {
      m_model = std::make_unique<GpuTracingEmissiveMaterialModel>(material);
    }

    std::unique_ptr<GpuTracingMaterialModel> takeModel() {
      return std::move(m_model);
    }

    const char* unsupportedReason() const {
      return m_unsupportedReason;
    }

  private:
    const char* m_unsupportedReason{"material type is not supported by GPU tracing scene compiler"};
    std::unique_ptr<GpuTracingMaterialModel> m_model;
  };

  std::unique_ptr<GpuTracingMaterialModel>
  lowerGpuTracingMaterialModel(const Material& material, std::string* unsupportedReason) {
    GpuTracingMaterialLoweringVisitor lowering;
    material.accept(lowering);
    std::unique_ptr<GpuTracingMaterialModel> model = lowering.takeModel();
    if (!model) {
      setUnsupportedReason(unsupportedReason, lowering.unsupportedReason());
    }
    return model;
  }

  std::optional<GpuTracingMaterialRecord>
  makeGpuTracingMaterialRecordWithResources(const Material& material,
                                            GpuTracingMaterialResourceContext& resources,
                                            std::string* unsupportedReason) {
    std::unique_ptr<GpuTracingMaterialModel> model =
      lowerGpuTracingMaterialModel(material, unsupportedReason);
    if (!model) {
      return std::nullopt;
    }
    return model->record(resources, unsupportedReason);
  }
}

std::array<GpuTracingSceneSectionLayout, 6> GpuTracingSceneSections::sectionLayouts() const {
  std::array<GpuTracingSceneSectionLayout, 6> layouts{};

  std::uint32_t offset = 0;
  const auto geometryBytes = byteCountFor(geometry.uploadByteCount());
  layouts[0] = GpuTracingSceneSectionLayout{GpuTracingSceneSectionKind::Geometry,
                                            gpuTracingSceneLayoutVersion,
                                            static_cast<std::uint32_t>(geometry.primitives.size()),
                                            0,
                                            16,
                                            offset,
                                            geometryBytes};
  offset += geometryBytes;

  layouts[1] = sectionLayout<GpuTracingMaterialRecord>(
    GpuTracingSceneSectionKind::Materials, static_cast<std::uint32_t>(materials.size()), offset);
  offset += layouts[1].byteCount;

  layouts[2] = sectionLayout<GpuTracingTextureRecord>(
    GpuTracingSceneSectionKind::Textures, static_cast<std::uint32_t>(textures.size()), offset);
  offset += layouts[2].byteCount;

  layouts[3] = sectionLayout<GpuTracingLightRecord>(
    GpuTracingSceneSectionKind::Lights, static_cast<std::uint32_t>(lights.size()), offset);
  offset += layouts[3].byteCount;

  layouts[4] = sectionLayout<GpuTracingEnvironmentRecord>(
    GpuTracingSceneSectionKind::Environment, static_cast<std::uint32_t>(environment.size()),
    offset);
  offset += layouts[4].byteCount;

  layouts[5] = sectionLayout<GpuTracingDebugIdRecord>(
    GpuTracingSceneSectionKind::DebugIds, static_cast<std::uint32_t>(debugIds.size()), offset);

  return layouts;
}

std::size_t GpuTracingSceneSections::uploadByteCount() const {
  return geometry.uploadByteCount() + materials.size() * sizeof(GpuTracingMaterialRecord) +
         textures.size() * sizeof(GpuTracingTextureRecord) +
         lights.size() * sizeof(GpuTracingLightRecord) +
         environment.size() * sizeof(GpuTracingEnvironmentRecord) +
         debugIds.size() * sizeof(GpuTracingDebugIdRecord);
}

std::vector<std::uint8_t> GpuTracingSceneSections::uploadBytes() const {
  std::vector<std::uint8_t> result;
  result.reserve(uploadByteCount());
  appendGeometryBytes(result, geometry);
  appendRecordBytes(result, materials);
  appendRecordBytes(result, textures);
  appendRecordBytes(result, lights);
  appendRecordBytes(result, environment);
  appendRecordBytes(result, debugIds);
  return result;
}

GpuTracingEnvironmentRecord render::makeGpuTracingConstantEnvironment(const Colord& color) {
  GpuTracingEnvironmentRecord record;
  record.color = color.toFloat4();
  return record;
}

bool GpuTracingLightCompilation::supported() const {
  return unsupportedLights.empty();
}

std::vector<GpuTracingUnsupportedReasonCount>
GpuTracingLightCompilation::unsupportedReasonCounts() const {
  return unsupportedReasonCountsFor(unsupportedLights);
}

bool GpuTracingTextureCompilation::supported() const {
  return unsupportedTextures.empty();
}

std::vector<GpuTracingUnsupportedReasonCount>
GpuTracingTextureCompilation::unsupportedReasonCounts() const {
  return unsupportedReasonCountsFor(unsupportedTextures);
}

bool GpuTracingMaterialCompilation::supported() const {
  return unsupportedMaterials.empty() && textures.supported();
}

std::vector<GpuTracingUnsupportedReasonCount>
GpuTracingMaterialCompilation::unsupportedReasonCounts() const {
  return unsupportedReasonCountsFor(unsupportedMaterials);
}

static std::optional<GpuTracingTextureRecord>
makeGpuTracingTextureRecordWithResources(const Texturec& texture,
                                         GpuTracingTextureResourceContext& resources,
                                         std::string* unsupportedReason) {
  if (const auto* constantColor = dynamic_cast<const ConstantColorTexture*>(&texture)) {
    GpuTracingTextureRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor);
    record.parameters = constantColor->color().toFloat4();
    return record;
  }

  if (const auto* checkerBoard = dynamic_cast<const CheckerBoardTexture*>(&texture)) {
    const TextureMapping2D* mapping = checkerBoard->mapping();
    const std::optional<std::uint32_t> flags = mappingFlagsFor(mapping, unsupportedReason);
    if (!flags) {
      return std::nullopt;
    }

    GpuTracingTextureRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingTextureKind::CheckerBoard);
    record.payloadOffset = resources.textureIdFor(checkerBoard->brightTexture());
    record.payloadCount = resources.textureIdFor(checkerBoard->darkTexture());
    record.flags = *flags;
    packTextureMappingParameters(mapping, record);
    return record;
  }

  if (const auto* image = dynamic_cast<const ImageTexture*>(&texture)) {
    if (image->filter() != ImageTextureFilter::Nearest) {
      setUnsupportedReason(unsupportedReason,
                           "image texture filter is not supported by GPU tracing scene compiler");
      return std::nullopt;
    }
    if (image->pixels().size() > std::numeric_limits<std::uint32_t>::max()) {
      setUnsupportedReason(unsupportedReason,
                           "image texture is too large for GPU tracing scene compiler");
      return std::nullopt;
    }

    const TextureMapping2D* mapping = image->mapping();
    const std::optional<std::uint32_t> flags = mappingFlagsFor(mapping, unsupportedReason);
    if (!flags) {
      return std::nullopt;
    }

    GpuTracingTextureRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingTextureKind::Image);
    record.payloadOffset = 0u;
    record.payloadCount = static_cast<std::uint32_t>(image->pixels().size());
    record.flags = *flags;
    if (image->wrap() == ImageTextureWrap::Clamp) {
      record.flags |= gpuTracingTextureWrapClampFlag;
    }
    packTextureMappingParameters(mapping, record);
    record.parameters[2] = static_cast<float>(image->width());
    record.parameters[3] = static_cast<float>(image->height());
    if (!image->pixels().empty()) {
      record.payloadOffset = resources.constantColorTexture(image->pixels().front());
      for (std::size_t index = 1; index != image->pixels().size(); ++index) {
        resources.constantColorTexture(image->pixels()[index]);
      }
    }
    return record;
  }

  setUnsupportedReason(unsupportedReason,
                       "texture type is not supported by GPU tracing scene compiler");
  return std::nullopt;
}

std::optional<GpuTracingTextureRecord>
render::makeGpuTracingTextureRecord(const Texturec& texture, std::string* unsupportedReason) {
  FunctionGpuTracingTextureResourceContext resources(
    [](const std::shared_ptr<Texturec>&) { return 0u; }, [](const Colord&) { return 0u; });
  return makeGpuTracingTextureRecordWithResources(texture, resources, unsupportedReason);
}

std::optional<GpuTracingMaterialRecord>
render::makeGpuTracingMaterialRecord(const Material& material, std::uint32_t albedoTexture,
                                     std::uint32_t emissionTexture,
                                     std::string* unsupportedReason) {
  FixedGpuTracingMaterialResourceContext resources(albedoTexture, emissionTexture);
  return makeGpuTracingMaterialRecordWithResources(material, resources, unsupportedReason);
}

GpuTracingMaterialCompilation
render::compileGpuTracingMaterials(const CompiledIntersectionScene& scene) {
  GpuTracingMaterialCompilation compilation;
  if (!scene.materials().empty()) {
    compilation.records.resize(scene.materials().size());
    compilation.textures.records.push_back(GpuTracingTextureRecord{});
  }

  std::map<const Texturec*, std::uint32_t> textureIds;
  auto appendConstantColorTexture = [&compilation](const Colord& color) {
    const std::uint32_t id = static_cast<std::uint32_t>(compilation.textures.records.size());
    compilation.textures.records.push_back(
      *makeGpuTracingTextureRecord(ConstantColorTexture(color)));
    return id;
  };

  std::function<std::uint32_t(const std::shared_ptr<Texturec>&)> textureIdFor;
  FunctionGpuTracingTextureResourceContext textureResources(
    [&textureIdFor](const std::shared_ptr<Texturec>& texture) { return textureIdFor(texture); },
    appendConstantColorTexture);
  textureIdFor = [&compilation, &textureIds,
                  &textureResources](const std::shared_ptr<Texturec>& texture) {
    if (!texture) {
      return 0u;
    }

    const auto existing = textureIds.find(texture.get());
    if (existing != textureIds.end()) {
      return existing->second;
    }

    const std::uint32_t id = static_cast<std::uint32_t>(compilation.textures.records.size());
    textureIds.emplace(texture.get(), id);
    compilation.textures.records.push_back(GpuTracingTextureRecord{});
    std::string reason;
    if (const std::optional<GpuTracingTextureRecord> record =
          makeGpuTracingTextureRecordWithResources(*texture, textureResources, &reason)) {
      compilation.textures.records[id] = *record;
    } else {
      compilation.textures.unsupportedTextures.push_back(
        UnsupportedGpuTracingTexture{id, texture->typeName(), reason});
    }
    return id;
  };
  FunctionGpuTracingMaterialResourceContext resources(textureIdFor, appendConstantColorTexture);

  for (std::uint32_t materialId = 1; materialId < scene.materials().size(); ++materialId) {
    const std::shared_ptr<Material>& material = scene.materials()[materialId];
    if (!material) {
      continue;
    }
    const Material& materialRef = *material;

    std::string reason;
    if (const std::optional<GpuTracingMaterialRecord> record =
          makeGpuTracingMaterialRecordWithResources(materialRef, resources, &reason)) {
      compilation.records[materialId] = *record;
    } else {
      compilation.records[materialId] = GpuTracingMaterialRecord{};
      compilation.unsupportedMaterials.push_back(
        UnsupportedGpuTracingMaterial{materialId, materialRef.typeName(), reason});
    }
  }

  return compilation;
}

std::optional<GpuTracingLightRecord>
render::makeGpuTracingLightRecord(const Light& light, std::string* unsupportedReason) {
  if (const auto* pointLight = dynamic_cast<const PointLight*>(&light)) {
    GpuTracingLightRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Point);
    record.positionOrDirection = pointLight->position().toFloat4(1.0f);
    record.parameters = pointLight->color().toFloat4();
    return record;
  }

  if (const auto* directionalLight = dynamic_cast<const DirectionalLight*>(&light)) {
    GpuTracingLightRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Directional);
    record.positionOrDirection = directionalLight->direction().toFloat4();
    record.parameters = directionalLight->color().toFloat4();
    return record;
  }

  if (const auto* areaLight = dynamic_cast<const RectangularAreaLight*>(&light)) {
    GpuTracingLightRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingLightKind::RectangularArea);
    record.positionOrDirection = areaLight->center().toFloat4(1.0f);
    record.u = areaLight->edgeU().toFloat4();
    record.v = areaLight->edgeV().toFloat4();
    record.parameters = areaLight->color().toFloat4();
    return record;
  }

  setUnsupportedReason(unsupportedReason,
                       "light type is not supported by GPU tracing scene compiler");
  return std::nullopt;
}

GpuTracingLightCompilation render::compileGpuTracingLights(const Scene& scene) {
  GpuTracingLightCompilation compilation;

  std::uint32_t lightIndex = 0;
  for (const std::shared_ptr<render::Light>& light : scene.lights()) {
    std::string unsupportedReason;
    if (const std::optional<GpuTracingLightRecord> record =
          makeGpuTracingLightRecord(*light, &unsupportedReason)) {
      compilation.records.push_back(*record);
    } else {
      compilation.unsupportedLights.push_back(
        UnsupportedGpuTracingLight{lightIndex, light->fingerprintType(), unsupportedReason});
    }

    if (lightIndex != std::numeric_limits<std::uint32_t>::max()) {
      ++lightIndex;
    }
  }

  return compilation;
}

bool GpuTracingSceneCompilation::supported() const {
  return diagnostics.unsupportedPrimitives == 0 && materials.supported() && lights.supported();
}

GpuDiffusePathLoopSupport
render::gpuDiffusePathLoopSupport(const GpuTracingSceneCompilation& compilation, const Scene&) {
  if (!compilation.supported()) {
    return {false, "GPU diffuse path loop requires a fully compiled GPU tracing scene"};
  }

  for (std::size_t materialId = 1; materialId < compilation.sections.materials.size();
       ++materialId) {
    const auto kind =
      static_cast<GpuTracingMaterialKind>(compilation.sections.materials[materialId].kind);
    if (kind != GpuTracingMaterialKind::Matte && kind != GpuTracingMaterialKind::Phong &&
        kind != GpuTracingMaterialKind::Emissive) {
      return {false,
              "GPU diffuse path loop supports only matte, Phong diffuse, and emissive materials"};
    }
  }

  return {true, {}};
}

bool render::supportsGpuDiffusePathLoop(const GpuTracingSceneCompilation& compilation,
                                        const Scene& scene) {
  return gpuDiffusePathLoopSupport(compilation, scene).supported;
}

std::string
render::gpuDiffusePathLoopUnsupportedReason(const GpuTracingSceneCompilation& compilation,
                                            const Scene& scene) {
  return gpuDiffusePathLoopSupport(compilation, scene).reason;
}

GpuTracingSceneCompilation
render::compileGpuTracingScene(const CompiledIntersectionScene& intersectionScene,
                               const Scene& scene) {
  GpuTracingSceneCompilation compilation;
  compilation.materials = compileGpuTracingMaterials(intersectionScene);
  compilation.lights = compileGpuTracingLights(scene);

  compilation.sections.geometry = GpuIntersectionScenePacker().packScene(intersectionScene);
  compilation.sections.materials = compilation.materials.records;
  compilation.sections.textures = compilation.materials.textures.records;
  compilation.sections.lights = compilation.lights.records;
  compilation.sections.environment.push_back(makeGpuTracingConstantEnvironment(scene.background()));
  if (scene.environmentRadiance() != scene.background()) {
    compilation.sections.environment.push_back(
      makeGpuTracingConstantEnvironment(scene.environmentRadiance()));
  }

  compilation.diagnostics.compiled = true;
  compilation.diagnostics.materials = compilation.sections.materials.size();
  compilation.diagnostics.textures = compilation.sections.textures.size();
  compilation.diagnostics.lights = compilation.sections.lights.size();
  compilation.diagnostics.environment = compilation.sections.environment.size();
  compilation.diagnostics.debugIds = compilation.sections.debugIds.size();
  compilation.diagnostics.unsupportedPrimitives = intersectionScene.unsupportedPrimitives().size();
  compilation.diagnostics.unsupportedMaterials = compilation.materials.unsupportedMaterials.size();
  compilation.diagnostics.unsupportedTextures =
    compilation.materials.textures.unsupportedTextures.size();
  compilation.diagnostics.unsupportedLights = compilation.lights.unsupportedLights.size();
  for (const UnsupportedIntersectionReasonCount& count :
       intersectionScene.unsupportedReasonCounts()) {
    compilation.diagnostics.unsupportedPrimitiveReasons[count.reason] = count.count;
  }
  insertReasonCounts(compilation.diagnostics.unsupportedMaterialReasons,
                     compilation.materials.unsupportedReasonCounts());
  insertReasonCounts(compilation.diagnostics.unsupportedTextureReasons,
                     compilation.materials.textures.unsupportedReasonCounts());
  insertReasonCounts(compilation.diagnostics.unsupportedLightReasons,
                     compilation.lights.unsupportedReasonCounts());
  compilation.diagnostics.uploadBytes = compilation.sections.uploadByteCount();
  return compilation;
}

GpuTracingSceneCompilation render::compileGpuTracingScene(const Scene& scene) {
  return compileGpuTracingScene(IntersectionSceneCompiler().compile(scene), scene);
}

GpuTracingSceneDiagnostics
render::compileGpuTracingSceneDiagnostics(const CompiledIntersectionScene& intersectionScene,
                                          const Scene& scene) {
  return compileGpuTracingScene(intersectionScene, scene).diagnostics;
}

GpuTracingSceneDiagnostics render::compileGpuTracingSceneDiagnostics(const Scene& scene) {
  return compileGpuTracingScene(scene).diagnostics;
}
