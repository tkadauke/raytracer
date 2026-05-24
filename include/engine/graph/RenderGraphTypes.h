#pragma once

#include <QJsonObject>

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace engine::graph {
  using RenderPassId = std::string;
  using RenderResourceId = std::string;
  using RenderFeatureKind = std::string;

  /**
    * Broad rendering backend preference used by `RenderIntent`.
    *
    * The compiler treats this as the default executor for the frame or view
    * override. Feature planners may still add other executors when the scene
    * asks for hybrid behavior, such as a wireframe inset or raster shadow map.
    */
  enum class RenderExecutorPreference {
    Raytracer,
    Rasterizer,
    Wireframe,
    // Add PathTracer here once a concrete path tracing engine exists.
  };

  /**
    * The high-level kind of view the user wants from a scene selection.
    *
    * Specific looks such as toon or clay shading are named shading profiles,
    * not enum values here. This enum stays limited to structural view modes.
    */
  enum class RenderViewMode {
    Default,
    Beauty,
    Wireframe,
    Depth,
    Normal,
    ObjectId
  };

  /**
    * Concrete executor required by a compiled render pass.
    */
  enum class RenderExecutorKind {
    Raytracer,
    Rasterizer,
    Wireframe,
    // Add PathTracer here once a concrete path tracing engine exists.
    Composite,
    PostProcess
  };

  /**
    * Coarse pass kind for UI grouping, CLI filters, and default disabling
    * behavior. The virtual pass payload carries the executor-specific details.
    */
  enum class RenderPassKind {
    Beauty,
    Shadow,
    Overlay,
    Composite,
    Tonemap,
    PostProcess,
    AOV,
    Debug,
    Custom
  };

  /**
    * How a compiled pass behaves when a user disables it before execution.
    */
  enum class DisabledBehavior {
    Error,
    CullDependents,
    SubstituteDefault,
    Passthrough
  };

  /**
    * Type of data stored behind a render resource handle.
    *
    * Keep this enum limited to resource families that the graph can allocate or
    * validate today. Future enum additions should include `EnvironmentMap`,
    * `IrradianceCache`, `PhotonMap`, `AccelerationData`, `LookupTable`,
    * `ScalarBuffer`, `Geometry`, `Volume`, and `ProceduralField` once those
    * pass/storage paths exist.
    */
  enum class RenderResourceType {
    Color,
    Depth,
    Stencil,
    ObjectId,
    MaterialId,
    Normal,
    WorldPosition,
    MotionVector,
    ShadowMap,
    ShadowMask,
    CustomTexture
  };

  /**
    * Where a resource physically lives. CPU is the only implemented domain for
    * the first graph slice; GPU keeps the descriptor model future-proof.
    */
  enum class RenderResourceDomain {
    CPU,
    GPU
  };

  /**
    * Lifetime class used by validation and future resource allocation.
    */
  enum class RenderResourceLifetime {
    Transient,
    Imported,
    Exported,
    History,
    PersistentCache
  };

  /**
    * Minimal format description for the first CPU-backed graph resources.
    */
  enum class RenderResourceFormat {
    Unknown,
    RGBDouble,
    DepthDouble,
    UInt8,
    UInt32,
    ScalarDouble
  };

  /**
    * Stable selector for applying render intent to part of the editable scene.
    */
  struct SceneSelector {
    enum class Kind {
      All,
      ObjectId,
      ObjectName,
      Tag,
      Layer,
      MaterialRole
    };

    Kind kind{Kind::All};
    std::string value;

    static SceneSelector all();
    static SceneSelector objectId(std::string id);
    static SceneSelector objectName(std::string name);
    static SceneSelector tag(std::string tagName);
    static SceneSelector layer(std::string layerName);
    static SceneSelector materialRole(std::string role);
  };

  /**
    * Named shading profile such as "default", "toon", "clay", or "xray".
    */
  struct ShadingProfileRef {
    std::string name{"default"};
    QJsonObject parameters;
  };

  /**
    * Placeholder for a serialized camera state. The first graph slice only
    * needs to distinguish scene-camera references from inline snapshots; fields
    * can grow when graph execution starts consuming alternate cameras.
    */
  struct CameraSnapshot {
    QJsonObject parameters;
  };

  /**
    * Camera reference for a whole frame or selected scene subset.
    */
  struct RenderCameraRef {
    std::optional<std::string> sceneCameraId;
    std::optional<CameraSnapshot> snapshot;
  };

  /**
    * Per-selector override layered on top of the scene's default render intent.
    */
  struct RenderViewOverride {
    SceneSelector selector;
    std::optional<RenderExecutorPreference> executor;
    std::optional<RenderViewMode> viewMode;
    std::optional<ShadingProfileRef> shadingProfile;
    std::optional<RenderCameraRef> camera;
  };

  /**
    * User-facing render request. It is serializable scene intent plus optional
    * caller overrides, not a low-level list of passes.
    */
  struct RenderIntent {
    RenderExecutorPreference defaultExecutor{RenderExecutorPreference::Raytracer};
    RenderViewMode defaultViewMode{RenderViewMode::Beauty};
    ShadingProfileRef defaultShadingProfile;
    std::optional<RenderCameraRef> defaultCamera;
    /// Allows the compiler to add feature-derived passes automatically.
    bool enableAutomaticFeatures{true};
    /// Adds a graph-visible wireframe overlay pass over the primary beauty image.
    bool enableWireframeOverlay{false};
    /// Enables low-cost preview shadows for rasterizer-backed preview graphs.
    bool enablePreviewShadows{false};
    std::vector<RenderViewOverride> viewOverrides;
  };

  /**
    * User or UI overrides applied to an already compiled graph.
    */
  struct RenderGraphOverrides {
    std::set<RenderPassId> disabledPasses;
    std::set<RenderPassKind> disabledPassKinds;
    std::set<RenderExecutorKind> disabledExecutors;
    std::set<RenderFeatureKind> disabledFeatures;
  };

  /**
    * Description of a resource in the compiled graph. Storage is allocated
    * separately so a future GPU domain can use the same plan model.
    */
  struct RenderResourceDescriptor {
    RenderResourceId id;
    std::string name;
    RenderResourceType type{RenderResourceType::Color};
    RenderResourceFormat format{RenderResourceFormat::Unknown};
    int width{0};
    int height{0};
    int sampleCount{1};
    RenderResourceDomain domain{RenderResourceDomain::CPU};
    RenderResourceLifetime lifetime{RenderResourceLifetime::Transient};
  };

  /**
    * Declares that a pass reads a graph resource.
    */
  struct ResourceRead {
    RenderResourceId resource;
  };

  /**
    * Declares that a pass writes a graph resource.
    */
  struct ResourceWrite {
    RenderResourceId resource;
  };

  /**
    * Scene subset and camera override for a compiled pass.
    */
  struct SceneView {
    SceneSelector selector{SceneSelector::all()};
    std::optional<RenderCameraRef> camera;
  };

  /**
    * A declarative node in the compiled render graph. It names inputs, outputs,
    * executor needs, and disabled behavior; implementation details live in the
    * pass payload.
    */
  struct RenderPassNode {
    RenderPassId id;
    std::string name;
    RenderPassKind kind{RenderPassKind::Custom};
    RenderExecutorKind executor{RenderExecutorKind::PostProcess};
    std::vector<RenderFeatureKind> features;
    std::vector<ResourceRead> reads;
    std::vector<ResourceWrite> writes;
    SceneView sceneView;
    DisabledBehavior disabledBehavior{DisabledBehavior::Error};
    bool enabled{true};
    bool hasExternalSideEffects{false};
    bool canRunConcurrently{true};
  };

  const char* toString(RenderExecutorPreference value);
  const char* toString(RenderViewMode value);
  const char* toString(RenderExecutorKind value);
  const char* toString(RenderPassKind value);
  const char* toString(DisabledBehavior value);
  const char* toString(RenderResourceType value);
  const char* toString(RenderResourceDomain value);
  const char* toString(RenderResourceLifetime value);
  const char* toString(RenderResourceFormat value);
  const char* toString(SceneSelector::Kind value);
}
