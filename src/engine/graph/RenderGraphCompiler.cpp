#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderAOV.h"
#include "engine/graph/RenderExecutor.h"
#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/WireframePassState.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::graph {
  namespace {
    RenderResourceId tonemapInputResource(const RenderPlan& plan) {
      const auto* tonemap = plan.findPass("tonemap");
      if (!tonemap) {
        throw std::runtime_error("compiled render graph is missing tonemap pass");
      }
      return tonemap->singleRead().resource;
    }

    std::string markerName(const RenderSceneAnalysis::SceneSurfaceMarker& marker) {
      return !marker.surfaceName.empty() ? marker.surfaceName : marker.surfaceId;
    }

    std::string portalSubviewName(const RenderSceneAnalysis::SceneSurfaceMarker& marker) {
      return "portal " + markerName(marker);
    }

    std::string mirrorSubviewName(const RenderSceneAnalysis::SceneSurfaceMarker& marker) {
      return "mirror " + markerName(marker);
    }

    void applyEngineOptionsToPass(RenderPassNode& pass, int rasterTargetSampleCount,
                                  const RenderIntent& intent) {
      if ((pass.executor == RenderExecutorKind::Raytracer ||
           pass.executor == RenderExecutorKind::Wavefront) &&
          pass.kind == RenderPassKind::Beauty) {
        intent.engineOptions.raytracer().beautyPassState().writeTo(pass);
      } else if (pass.executor == RenderExecutorKind::Rasterizer) {
        intent.engineOptions.rasterizer()
          .beautyPassState(rasterTargetSampleCount, intent.postProcessAA,
                           !intent.usesGraphImagePostProcessAA(), false)
          .writeTo(pass);
      } else if (pass.executor == RenderExecutorKind::Wireframe) {
        intent.engineOptions.wireframe().passState().writeTo(pass);
      }
    }

    RenderPassNode aovVisualizationPass(const RenderAOVDefinition& aov,
                                        const RenderResourceId& inputResource,
                                        const RenderResourceId& outputResource, bool mainPass) {
      RenderPassNode pass;
      pass.id = "visualize_" + aov.resourceId();
      pass.name = "Visualize " + aov.title() + " AOV";
      pass.kind = RenderPassKind::AOV;
      pass.executor = RenderExecutorKind::PostProcess;
      pass.features = mainPass ? std::vector<RenderFeatureKind>{"main", "aov", aov.feature(),
                                                                "visualization", "postprocess"}
                               : std::vector<RenderFeatureKind>{"aov", "export", aov.feature(),
                                                                "visualization", "postprocess"};
      pass.addRead(inputResource);
      pass.addWrite(outputResource);
      pass.sceneView.selector = SceneSelector::all();
      pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
      pass.concurrency = RenderConcurrencyLimit::serial();
      pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
      return pass;
    }

    bool passCanSampleSubviewTextures(const RenderPassNode& pass) {
      if (pass.hasFeature("subview")) {
        return false;
      }
      if (pass.executor != RenderExecutorKind::Raytracer &&
          pass.executor != RenderExecutorKind::Wavefront &&
          pass.executor != RenderExecutorKind::Rasterizer) {
        return false;
      }
      return pass.kind == RenderPassKind::Beauty || pass.kind == RenderPassKind::AOV;
    }

    struct TracingExecutionDecision {
      TracingExecutionPreference requested{TracingExecutionPreference::Auto};
      TracingExecutionPreference predicted{TracingExecutionPreference::CPU};
      render::WavefrontIntersectionBackendChoice intersectionBackend =
        render::WavefrontIntersectionBackendChoice::automatic();
      bool overrideIntersectionBackend{false};
      std::string fallbackReason;
    };

    bool isTracingExecutor(RenderExecutorKind executor) {
      return executor == RenderExecutorKind::Raytracer || executor == RenderExecutorKind::Wavefront;
    }

    bool canUseHybridTracing(RenderExecutorKind executor) {
      return executor == RenderExecutorKind::Wavefront;
    }

    bool canUseFullGpuTracing(const RenderSceneAnalysis& sceneAnalysis) {
      return sceneAnalysis.fullGpuTracingSupported() &&
             sceneAnalysis.fullGpuTracingBackendAvailable();
    }

    std::string fullGpuTracingFallbackReason(const RenderSceneAnalysis& sceneAnalysis) {
      if (!sceneAnalysis.fullGpuTracingBackendAvailable()) {
        return sceneAnalysis.fullGpuTracingBackendUnavailableReason();
      }
      return sceneAnalysis.fullGpuTracingUnsupportedReason();
    }

    TracingExecutionDecision resolveTracingExecution(const RenderIntent& intent,
                                                     RenderExecutorKind executor,
                                                     const RenderSceneAnalysis& sceneAnalysis) {
      TracingExecutionDecision decision;
      const auto requested = intent.engineOptions.raytracer().tracingExecution();
      const auto intersectionBackend = intent.engineOptions.raytracer().intersectionBackend();
      decision.requested = requested.value_or(TracingExecutionPreference::Auto);
      if (intersectionBackend) {
        decision.intersectionBackend = *intersectionBackend;
      }

      if (decision.requested == TracingExecutionPreference::CPU) {
        if (intersectionBackend &&
            intersectionBackend->kind() == render::WavefrontIntersectionBackendChoice::Kind::GPU) {
          throw std::runtime_error(
            "tracingExecution 'cpu' is incompatible with intersectionBackend 'gpu'");
        }
        decision.predicted = TracingExecutionPreference::CPU;
        decision.intersectionBackend = render::WavefrontIntersectionBackendChoice::cpu();
        decision.overrideIntersectionBackend = true;
        return decision;
      }

      if (decision.requested == TracingExecutionPreference::GPU) {
        if (intersectionBackend &&
            intersectionBackend->kind() == render::WavefrontIntersectionBackendChoice::Kind::CPU &&
            canUseFullGpuTracing(sceneAnalysis)) {
          throw std::runtime_error(
            "tracingExecution 'gpu' is incompatible with intersectionBackend 'cpu'");
        }
        if (canUseFullGpuTracing(sceneAnalysis)) {
          decision.predicted = TracingExecutionPreference::GPU;
          decision.intersectionBackend = render::WavefrontIntersectionBackendChoice::gpu();
          decision.overrideIntersectionBackend = true;
        } else if (canUseHybridTracing(executor)) {
          decision.predicted = TracingExecutionPreference::Hybrid;
          decision.intersectionBackend =
            intersectionBackend.value_or(render::WavefrontIntersectionBackendChoice::gpu());
          decision.overrideIntersectionBackend = !intersectionBackend.has_value();
          decision.fallbackReason = fullGpuTracingFallbackReason(sceneAnalysis);
        } else {
          decision.predicted = TracingExecutionPreference::CPU;
          decision.intersectionBackend = render::WavefrontIntersectionBackendChoice::cpu();
          decision.overrideIntersectionBackend = true;
          decision.fallbackReason = fullGpuTracingFallbackReason(sceneAnalysis);
        }
        return decision;
      }

      if (decision.requested == TracingExecutionPreference::Hybrid) {
        if (canUseHybridTracing(executor)) {
          if (intersectionBackend && intersectionBackend->kind() ==
                                       render::WavefrontIntersectionBackendChoice::Kind::CPU) {
            decision.predicted = TracingExecutionPreference::CPU;
            decision.fallbackReason =
              "hybrid tracing requested but the intersection backend is forced to CPU";
          } else {
            decision.predicted = TracingExecutionPreference::Hybrid;
            decision.intersectionBackend =
              intersectionBackend.value_or(render::WavefrontIntersectionBackendChoice::gpu());
            decision.overrideIntersectionBackend = !intersectionBackend.has_value();
          }
        } else {
          decision.predicted = TracingExecutionPreference::CPU;
          decision.intersectionBackend = render::WavefrontIntersectionBackendChoice::cpu();
          decision.overrideIntersectionBackend = true;
          decision.fallbackReason = "hybrid tracing requires a wavefront tracing schedule";
        }
        return decision;
      }

      if (canUseFullGpuTracing(sceneAnalysis) && canUseHybridTracing(executor)) {
        decision.predicted = TracingExecutionPreference::GPU;
        decision.intersectionBackend = render::WavefrontIntersectionBackendChoice::gpu();
        decision.overrideIntersectionBackend = !intersectionBackend.has_value();
      } else if (canUseHybridTracing(executor) && intersectionBackend &&
                 intersectionBackend->kind() ==
                   render::WavefrontIntersectionBackendChoice::Kind::GPU) {
        decision.predicted = TracingExecutionPreference::Hybrid;
      } else {
        decision.predicted = TracingExecutionPreference::CPU;
      }
      return decision;
    }

    void applyTracingExecutionDecision(RenderPassNode& pass,
                                       const TracingExecutionDecision& decision) {
      if (!isTracingExecutor(pass.executor)) {
        return;
      }
      RaytracerBeautyPassState state = RaytracerBeautyPassState::valueFromPass(pass);
      state.setPredictedTracingExecution(decision.predicted);
      state.setTracingExecutionFallbackReason(decision.fallbackReason);
      if (decision.overrideIntersectionBackend) {
        state.setIntersectionBackend(decision.intersectionBackend);
      }
      state.writeTo(pass);
      pass.features.push_back(std::string("tracing_execution_") +
                              tracingExecutionPreferenceName(decision.predicted));
      if (!decision.fallbackReason.empty()) {
        pass.features.push_back("tracing_execution_fallback");
      }
    }

    RenderPassNode aovProducerPass(const RenderAOVDefinition& aov, RenderExecutorKind executor,
                                   const SceneView& sceneView, bool mainPass,
                                   const RenderTargetSpec& target, const RenderIntent& intent,
                                   const RenderSceneAnalysis& sceneAnalysis) {
      const auto* executorDefinition = renderExecutorDefinition(executor);
      if (!executorDefinition) {
        throw std::runtime_error("executor cannot produce AOV pass");
      }
      if (!aov.supportsExecutor(executor)) {
        throw std::runtime_error("view mode '" + std::string(toString(aov.viewMode())) +
                                 "' is not supported by executor '" +
                                 std::string(toString(executor)) + "'");
      }
      RenderPassNode pass;
      pass.id = aov.resourceId();
      pass.name = aov.title() + " AOV";
      pass.kind = RenderPassKind::AOV;
      pass.executor = executor;
      pass.features = mainPass ? std::vector<RenderFeatureKind>{"main", "aov", aov.feature(),
                                                                executorDefinition->feature()}
                               : std::vector<RenderFeatureKind>{"aov", "export", aov.feature(),
                                                                executorDefinition->feature()};
      pass.sceneView = sceneView;
      pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
      pass.concurrency = RenderConcurrencyLimit::serial();
      pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
      applyEngineOptionsToPass(pass, aov.usesRasterTargetSampling() ? target.sampleCount : 1,
                               intent);
      if (aov.usesRaytracerPassState()) {
        intent.engineOptions.raytracer().beautyPassState().writeTo(pass);
      }
      aov.configureProducerPass(pass);
      applyTracingExecutionDecision(pass,
                                    resolveTracingExecution(intent, pass.executor, sceneAnalysis));
      return pass;
    }
  }

  RenderTargetSpec RenderTargetSpec::normalized() const {
    RenderTargetSpec result = *this;
    result.sampleCount = std::max(1, result.sampleCount);
    return result;
  }

  RenderResourceDescriptor RenderTargetSpec::colorResource(RenderResourceId id, std::string name,
                                                           RenderResourceLifetime lifetime) const {
    RenderResourceDescriptor color;
    color.id = std::move(id);
    color.name = std::move(name);
    color.type = RenderResourceType::Color;
    color.format = RenderResourceFormat::RGBDouble;
    color.width = width;
    color.height = height;
    color.sampleCount = sampleCount;
    color.domain = RenderResourceDomain::CPU;
    color.lifetime = lifetime;
    return color;
  }

  RenderPassNode RenderGraphCompiler::beautyPass(
    const RenderExecutorDefinition& executorDefinition, const SceneView& sceneView,
    const RenderTargetSpec& target, const RenderIntent& intent,
    const RenderSceneAnalysis& sceneAnalysis, std::vector<RenderFeatureKind> extraFeatures) const {
    RenderPassNode pass;
    pass.id = executorDefinition.beautyPassId();
    pass.name = executorDefinition.beautyPassName();
    pass.kind = RenderPassKind::Beauty;
    pass.executor = executorDefinition.kind();
    pass.features = {"main", "beauty", executorDefinition.feature()};
    pass.features.insert(pass.features.end(), extraFeatures.begin(), extraFeatures.end());
    pass.sceneView = sceneView;
    pass.disabledBehavior = DisabledBehavior::Error;
    pass.concurrency = RenderConcurrencyLimit::serial();
    pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
    executorDefinition.configureBeautyPassState(pass, target.sampleCount, intent);
    applyTracingExecutionDecision(pass,
                                  resolveTracingExecution(intent, pass.executor, sceneAnalysis));
    return pass;
  }

  bool RenderGraphCompiler::beautyPassNeedsExplicitReadback(const RenderPassNode& pass) const {
    return pass.kind == RenderPassKind::Beauty && passNeedsExplicitReadback(pass);
  }

  bool RenderGraphCompiler::passNeedsExplicitReadback(const RenderPassNode& pass) const {
    const auto* state = RasterBeautyPassState::fromPass(pass);
    return state && state->execution().backend().isOpenGL();
  }

  bool RenderGraphCompiler::rasterVisibilityCullingRequested(const RenderIntent& intent) const {
    const auto mode = intent.engineOptions.rasterizer().visibilityCulling();
    return mode && *mode != RenderVisibilityCulling::Off;
  }

  RenderResourceDescriptor
  RenderGraphCompiler::visibilitySetResource(const RenderTargetSpec& target) const {
    RenderResourceDescriptor resource;
    resource.id = "raster_visibility_set";
    resource.name = "Raster visibility set";
    resource.addFeature("visibility");
    resource.addFeature("culling");
    resource.addFeature("rasterizer");
    resource.type = RenderResourceType::VisibilitySet;
    resource.format = RenderResourceFormat::Unknown;
    resource.width = target.width;
    resource.height = target.height;
    resource.sampleCount = 1;
    resource.domain = RenderResourceDomain::CPU;
    resource.lifetime = RenderResourceLifetime::PersistentCache;
    return resource;
  }

  RenderPassNode RenderGraphCompiler::visibilityCullingPass(const SceneView& sceneView,
                                                            const RenderIntent& intent) const {
    RenderPassNode pass;
    pass.id = "raster_visibility";
    pass.name = "Raster visibility culling";
    pass.kind = RenderPassKind::Visibility;
    pass.executor = RenderExecutorKind::Rasterizer;
    pass.features = {"main", "visibility", "culling", "rasterizer"};
    pass.sceneView = sceneView;
    pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
    pass.concurrency = RenderConcurrencyLimit::serial();
    pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
    intent.engineOptions.rasterizer().visibilityPassState().writeTo(pass);
    return pass;
  }

  void RenderGraphCompiler::addRasterVisibilityInput(RenderPlan& plan,
                                                     const RenderTargetSpec& target,
                                                     RenderPassNode& pass,
                                                     const SceneView& sceneView,
                                                     const RenderIntent& intent) const {
    if (pass.executor != RenderExecutorKind::Rasterizer ||
        !rasterVisibilityCullingRequested(intent)) {
      return;
    }

    const RenderResourceId visibilityResource = "raster_visibility_set";
    if (!plan.findResource(visibilityResource)) {
      plan.addResourceProducer(visibilityCullingPass(sceneView, intent),
                               visibilitySetResource(target));
    }
    pass.addRead(visibilityResource);
  }

  RenderResourceDescriptor
  RenderGraphCompiler::hybridShadowMaskResource(const RenderTargetSpec& target) const {
    RenderResourceDescriptor resource;
    resource.id = "hybrid_shadow_mask";
    resource.name = "Hybrid ray-traced shadow mask";
    resource.addFeature("preview_shadows");
    resource.addFeature("ray_traced_shadows");
    resource.addFeature("hybrid_visibility");
    resource.type = RenderResourceType::ShadowMask;
    resource.format = RenderResourceFormat::RGBDouble;
    resource.width = target.width;
    resource.height = target.height;
    resource.sampleCount = 1;
    resource.domain = RenderResourceDomain::CPU;
    resource.lifetime = RenderResourceLifetime::Transient;
    return resource;
  }

  RenderPassNode RenderGraphCompiler::hybridShadowMaskPass(const SceneView& sceneView,
                                                           const RenderIntent& intent) const {
    RenderPassNode pass;
    pass.id = "hybrid_ray_traced_shadows";
    pass.name = "Hybrid ray-traced shadows";
    pass.kind = RenderPassKind::Shadow;
    pass.executor = RenderExecutorKind::Raytracer;
    pass.features = {"main", "preview_shadows", "ray_traced_shadows", "hybrid_visibility"};
    pass.sceneView = sceneView;
    pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
    pass.concurrency = RenderConcurrencyLimit::serial();
    pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
    intent.engineOptions.raytracer().beautyPassState().writeTo(pass);
    return pass;
  }

  RenderPassNode
  RenderGraphCompiler::hybridShadowCompositePass(RenderResourceId colorResource,
                                                 RenderResourceId maskResource,
                                                 RenderResourceId outputResource) const {
    RenderPassNode pass;
    pass.id = "hybrid_shadow_composite";
    pass.name = "Hybrid shadow composite";
    pass.kind = RenderPassKind::Composite;
    pass.executor = RenderExecutorKind::Composite;
    pass.features = {"main", "composite", "preview_shadows", "ray_traced_shadows",
                     "hybrid_visibility"};
    pass.addRead(std::move(colorResource));
    pass.addRead(std::move(maskResource));
    pass.addWrite(std::move(outputResource));
    pass.sceneView.selector = SceneSelector::all();
    pass.disabledBehavior = DisabledBehavior::Passthrough;
    pass.concurrency = RenderConcurrencyLimit::serial();
    pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
    return pass;
  }

  RenderResourceId
  RenderGraphCompiler::addHybridShadowComposite(RenderPlan& plan, const RenderTargetSpec& target,
                                                RenderResourceId inputResource) const {
    const RenderResourceId outputResource = "hybrid_shadowed_color";
    RenderPassNode composite =
      hybridShadowCompositePass(inputResource, "hybrid_shadow_mask", outputResource);
    RenderResourceDescriptor output = target.colorResource(outputResource, "Hybrid shadowed color",
                                                           RenderResourceLifetime::Transient);
    plan.routeResourceThroughPass(inputResource, std::move(output), std::move(composite));
    return outputResource;
  }

  RenderResourceDescriptor
  RenderGraphCompiler::readbackResource(const RenderResourceDescriptor& source, RenderResourceId id,
                                        std::string name, RenderResourceLifetime lifetime) const {
    RenderResourceDescriptor result = source;
    result.id = std::move(id);
    result.name = std::move(name);
    result.lifetime = lifetime;
    return result;
  }

  RenderPassNode
  RenderGraphCompiler::readbackPass(RenderPassId id, std::string name,
                                    RenderResourceId inputResource, RenderResourceId outputResource,
                                    std::vector<RenderFeatureKind> baseFeatures,
                                    std::vector<RenderFeatureKind> extraFeatures) const {
    RenderPassNode pass;
    pass.id = std::move(id);
    pass.name = std::move(name);
    pass.kind = RenderPassKind::Readback;
    pass.executor = RenderExecutorKind::PostProcess;
    pass.features = std::move(baseFeatures);
    pass.features.push_back("readback");
    pass.features.push_back("transfer");
    pass.features.insert(pass.features.end(), extraFeatures.begin(), extraFeatures.end());
    pass.addRead(std::move(inputResource));
    pass.addWrite(std::move(outputResource));
    pass.supportedResourceDomains = {RenderResourceDomain::CPU, RenderResourceDomain::GPU};
    pass.sceneView.selector = SceneSelector::all();
    pass.disabledBehavior = DisabledBehavior::Error;
    pass.concurrency = RenderConcurrencyLimit::serial();
    pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
    return pass;
  }

  RenderPassNode RenderGraphCompiler::readbackPass(RenderResourceId inputResource,
                                                   RenderResourceId outputResource) const {
    return readbackPass("beauty_readback", "Beauty readback", std::move(inputResource),
                        std::move(outputResource), {"main"});
  }

  RenderPassNode RenderGraphCompiler::tonemapPass(RenderResourceId inputResource,
                                                  RenderResourceId outputResource) const {
    RenderPassNode pass;
    pass.id = "tonemap";
    pass.name = "Tone map";
    pass.kind = RenderPassKind::Tonemap;
    pass.executor = RenderExecutorKind::PostProcess;
    pass.features = {"main", "tonemap", "postprocess"};
    pass.addRead(std::move(inputResource));
    pass.addWrite(std::move(outputResource));
    pass.sceneView.selector = SceneSelector::all();
    pass.disabledBehavior = DisabledBehavior::Passthrough;
    pass.concurrency = RenderConcurrencyLimit::serial();
    pass.canRunConcurrently = pass.concurrency.allowsParallelExecution();
    return pass;
  }

  RenderResourceDescriptor
  RenderGraphCompiler::selectorColorResource(const RenderTargetSpec& target, RenderResourceId id,
                                             std::string name) const {
    RenderResourceDescriptor resource =
      target.colorResource(std::move(id), std::move(name), RenderResourceLifetime::Transient);
    resource.addFeature("selector_override");
    return resource;
  }

  RenderIntent
  RenderGraphCompiler::selectorOverrideIntent(const RenderIntent& frameIntent,
                                              const RenderViewOverride& viewOverride) const {
    RenderIntent branchIntent = frameIntent;
    branchIntent.viewOverrides.clear();
    branchIntent.subviews.clear();
    if (viewOverride.executor) {
      branchIntent.setDefaultExecutor(*viewOverride.executor);
    }
    if (viewOverride.viewMode && *viewOverride.viewMode != RenderViewMode::Default) {
      branchIntent.setDefaultViewMode(*viewOverride.viewMode);
    }
    if (viewOverride.shadingProfile) {
      branchIntent.setDefaultShadingProfile(*viewOverride.shadingProfile);
    }
    if (viewOverride.camera) {
      branchIntent.setDefaultCamera(*viewOverride.camera);
    }
    if (!viewOverride.engineOptions.empty()) {
      branchIntent.engineOptions =
        viewOverride.inheritEngineOptions.value_or(true)
          ? branchIntent.engineOptions.mergedWith(viewOverride.engineOptions)
          : viewOverride.engineOptions;
    }
    return branchIntent;
  }

  SceneView
  RenderGraphCompiler::selectorOverrideSceneView(const RenderIntent& branchIntent,
                                                 const RenderViewOverride& viewOverride) const {
    SceneView sceneView = branchIntent.defaultSceneView();
    sceneView.selector = viewOverride.selector;
    return sceneView;
  }

  RenderPassNode RenderGraphCompiler::selectorCompositePass(
    RenderPassId id, std::string name, RenderResourceId baseResource,
    RenderResourceId foregroundResource, RenderResourceId stencilResource,
    RenderResourceId outputResource, const SceneView& sceneView,
    std::vector<RenderFeatureKind> features) const {
    RenderPassNode composite;
    composite.id = std::move(id);
    composite.name = std::move(name);
    composite.kind = RenderPassKind::Composite;
    composite.executor = RenderExecutorKind::Composite;
    composite.features = {"main", "composite", "selector_override"};
    composite.features.insert(composite.features.end(), features.begin(), features.end());
    composite.addRead(std::move(baseResource));
    composite.addRead(std::move(foregroundResource));
    composite.addRead(std::move(stencilResource));
    composite.addWrite(std::move(outputResource));
    composite.sceneView = sceneView;
    composite.disabledBehavior = DisabledBehavior::SubstituteDefault;
    composite.canRunConcurrently = false;
    return composite;
  }

  RenderResourceId RenderGraphCompiler::addSelectorOverrideBranches(
    RenderPlan& plan, const RenderTargetSpec& target, RenderResourceId baseInputResource,
    const RenderIntent& frameIntent, const RenderSceneAnalysis& sceneAnalysis) const {
    const auto overrides = frameIntent.selectorSpecificOverrides();
    if (overrides.empty()) {
      return baseInputResource;
    }

    std::set<std::pair<SceneSelector::Kind, std::string>> seenSelectors;
    for (const auto& viewOverride : overrides) {
      const auto key = std::make_pair(viewOverride.selector.kind, viewOverride.selector.value);
      if (!seenSelectors.insert(key).second) {
        throw std::runtime_error("RenderGraphCompiler has conflicting selector-specific render "
                                 "intent for " +
                                 viewOverride.selector.displayText());
      }
    }

    RenderResourceId currentBase = std::move(baseInputResource);
    for (std::size_t i = 0; i != overrides.size(); ++i) {
      const auto match = sceneAnalysis.matchSelector(overrides[i].selector);
      if (!match.matched()) {
        continue;
      }
      currentBase = addSelectorOverrideBranch(plan, target, std::move(currentBase), frameIntent,
                                              overrides[i], i, sceneAnalysis);
    }
    return currentBase;
  }

  RenderResourceId RenderGraphCompiler::addSelectorOverrideBranch(
    RenderPlan& plan, const RenderTargetSpec& target, RenderResourceId baseInputResource,
    const RenderIntent& frameIntent, const RenderViewOverride& viewOverride,
    std::size_t overrideIndex, const RenderSceneAnalysis& sceneAnalysis) const {
    const RenderIntent branchIntent = selectorOverrideIntent(frameIntent, viewOverride);
    const SceneView sceneView = selectorOverrideSceneView(branchIntent, viewOverride);
    const std::string prefix = "selector_" + std::to_string(overrideIndex + 1);

    if (branchIntent.defaultViewMode == RenderViewMode::StencilComposite) {
      throw std::runtime_error("RenderGraphCompiler does not support selector-specific "
                               "stencil_composite view yet (" +
                               viewOverride.selector.displayText() + ")");
    }

    const auto* stencilAOV = renderAOVDefinition(RenderViewMode::Stencil);
    if (!stencilAOV) {
      throw std::runtime_error("selector-specific render intent requires a stencil AOV definition");
    }

    const RenderResourceId stencilId = prefix + "_stencil_aov";
    RenderPassNode stencilProducer =
      aovProducerPass(*stencilAOV, RenderExecutorKind::Rasterizer, sceneView, false, target,
                      branchIntent, sceneAnalysis);
    stencilProducer.id = stencilId;
    stencilProducer.name = "Selector " + std::to_string(overrideIndex + 1) + " stencil mask";
    stencilProducer.features.push_back("selector_override");
    addRasterVisibilityInput(plan, target, stencilProducer, sceneView, branchIntent);
    RenderResourceDescriptor stencilResource =
      stencilAOV->resourceDescriptor(target, RenderResourceLifetime::Transient);
    stencilResource.id = stencilId;
    stencilResource.name = stencilProducer.name;
    stencilResource.addFeature("selector_override");
    plan.addResourceProducer(std::move(stencilProducer), std::move(stencilResource));

    RenderResourceId foregroundInput;
    std::vector<RenderFeatureKind> compositeFeatures;
    if (const auto* aov = renderAOVDefinition(branchIntent.defaultViewMode)) {
      const RenderExecutorKind executor = branchIntent.defaultExecutorKind();
      const RenderResourceId aovId = prefix + "_" + aov->resourceId();
      RenderPassNode producer =
        aovProducerPass(*aov, executor, sceneView, false, target, branchIntent, sceneAnalysis);
      producer.id = aovId;
      producer.name = "Selector " + std::to_string(overrideIndex + 1) + " " + aov->title() + " AOV";
      producer.features.push_back("selector_override");
      addRasterVisibilityInput(plan, target, producer, sceneView, branchIntent);
      RenderResourceDescriptor aovResource =
        aov->resourceDescriptor(target, RenderResourceLifetime::Exported);
      aovResource.id = aovId;
      aovResource.name = producer.name;
      aovResource.addFeature("selector_override");
      plan.addResourceProducer(std::move(producer), std::move(aovResource));

      const RenderResourceId previewId = prefix + "_" + aov->previewColorResourceId();
      RenderPassNode visualization = aovVisualizationPass(*aov, aovId, previewId, false);
      visualization.id = prefix + "_" + visualization.id;
      visualization.name =
        "Selector " + std::to_string(overrideIndex + 1) + " " + visualization.name;
      visualization.sceneView = sceneView;
      visualization.features.push_back("selector_override");
      RenderResourceDescriptor preview = target.colorResource(
        previewId,
        "Selector " + std::to_string(overrideIndex + 1) + " " + aov->title() + " AOV preview",
        RenderResourceLifetime::Exported);
      preview.addFeature("selector_override");
      plan.addResourceProducer(std::move(visualization), std::move(preview));
      foregroundInput = previewId;
      compositeFeatures = {"aov", aov->feature()};
    } else {
      const RenderExecutorKind executor = branchIntent.defaultExecutorKind();
      const auto* concreteExecutorDefinition = renderExecutorDefinition(executor);
      if (!concreteExecutorDefinition) {
        throw std::runtime_error("selector-specific render intent executor cannot produce a "
                                 "beauty pass");
      }
      const auto& preferredExecutorDefinition =
        renderExecutorDefinition(branchIntent.defaultExecutor);
      const RenderExecutorDefinition& beautyExecutorDefinition =
        preferredExecutorDefinition.kind() == executor ? preferredExecutorDefinition
                                                       : *concreteExecutorDefinition;
      const RenderResourceId colorId = prefix + "_beauty_color";
      RenderPassNode foreground = beautyPass(beautyExecutorDefinition, sceneView, target,
                                             branchIntent, sceneAnalysis, {"selector_override"});
      foreground.id = prefix + "_" + foreground.id;
      foreground.name = "Selector " + std::to_string(overrideIndex + 1) + " " + foreground.name;
      addRasterVisibilityInput(plan, target, foreground, sceneView, branchIntent);
      plan.addResourceProducer(
        std::move(foreground),
        selectorColorResource(target, colorId,
                              "Selector " + std::to_string(overrideIndex + 1) + " beauty color"));
      foregroundInput = colorId;
      compositeFeatures = {"beauty"};
    }

    const RenderResourceId outputId = prefix + "_composited_color";
    RenderPassNode composite = selectorCompositePass(
      prefix + "_composite", "Selector " + std::to_string(overrideIndex + 1) + " composite",
      baseInputResource, foregroundInput, stencilId, outputId, sceneView, compositeFeatures);
    plan.addResourceProducer(
      std::move(composite),
      selectorColorResource(target, outputId,
                            "Selector " + std::to_string(overrideIndex + 1) + " composited color"));
    return outputId;
  }

  RenderPlan RenderGraphCompiler::aovViewPlan(const RenderTargetSpec& target,
                                              RenderExecutorKind executor,
                                              const RenderAOVDefinition& aov,
                                              const SceneView& sceneView,
                                              const RenderIntent& intent,
                                              const RenderSceneAnalysis& sceneAnalysis) const {
    RenderPlan plan;

    const RenderResourceId aovId = aov.resourceId();
    RenderPassNode producer =
      aovProducerPass(aov, executor, sceneView, true, target, intent, sceneAnalysis);
    addRasterVisibilityInput(plan, target, producer, sceneView, intent);
    RenderResourceDescriptor aovResource =
      aov.resourceDescriptor(target, RenderResourceLifetime::Transient);
    plan.addResourceProducer(producer, aovResource);

    RenderResourceId visualizationInput = aovId;
    if (passNeedsExplicitReadback(producer)) {
      const RenderResourceId readbackId = aovId + "_readback";
      RenderResourceDescriptor readbackDescriptor = readbackResource(
        aovResource, readbackId, aov.title() + " AOV readback", RenderResourceLifetime::Transient);
      RenderPassNode readback =
        readbackPass("readback_" + aovId, "Read back " + aov.title() + " AOV", aovId, readbackId,
                     {"main", "aov", aov.feature()});
      plan.addResourceProducer(std::move(readback), std::move(readbackDescriptor));
      visualizationInput = readbackId;
    }

    RenderResourceDescriptor mainColor =
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported);
    plan.routeResourceThroughPass(
      visualizationInput, mainColor,
      aovVisualizationPass(aov, visualizationInput, mainColor.id, true));
    return plan;
  }

  void RenderGraphCompiler::addAuxiliaryAOVExport(
    RenderPlan& plan, const RenderTargetSpec& target, RenderExecutorKind executor,
    RenderViewMode viewMode, RenderViewMode defaultViewMode, const SceneView& sceneView,
    const RenderIntent& intent, const RenderSceneAnalysis& sceneAnalysis) const {
    const auto* aov = renderAOVDefinition(viewMode);
    if (!aov) {
      throw std::runtime_error("view mode '" + std::string(toString(viewMode)) +
                               "' is not an AOV export");
    }

    if (viewMode == defaultViewMode || plan.findResource(aov->previewColorResourceId())) {
      return;
    }

    const RenderResourceId aovId = aov->resourceId();
    const RenderResourceId previewId = aov->previewColorResourceId();
    RenderResourceId visualizationInput = aovId;
    if (!plan.findResource(aovId)) {
      RenderPassNode producer =
        aovProducerPass(*aov, executor, sceneView, false, target, intent, sceneAnalysis);
      addRasterVisibilityInput(plan, target, producer, sceneView, intent);
      if (passNeedsExplicitReadback(producer)) {
        const RenderResourceId sourceId = aovId + "_source";
        RenderResourceDescriptor sourceDescriptor = readbackResource(
          aov->resourceDescriptor(target, RenderResourceLifetime::Transient), sourceId,
          aov->title() + " AOV source", RenderResourceLifetime::Transient);
        plan.addResourceProducer(producer, std::move(sourceDescriptor));

        RenderPassNode readback =
          readbackPass("readback_" + aovId, "Read back " + aov->title() + " AOV", sourceId, aovId,
                       {"aov", "export", aov->feature()});
        plan.addResourceProducer(std::move(readback),
                                 aov->resourceDescriptor(target, RenderResourceLifetime::Exported));
      } else {
        plan.addResourceProducer(std::move(producer),
                                 aov->resourceDescriptor(target, RenderResourceLifetime::Exported));
      }
    }
    plan.addResourceProducer(aovVisualizationPass(*aov, visualizationInput, previewId, false),
                             target.colorResource(previewId, aov->title() + " AOV preview",
                                                  RenderResourceLifetime::Exported));
  }

  void RenderGraphCompiler::addAuxiliaryAOVExports(RenderPlan& plan, const RenderTargetSpec& target,
                                                   RenderExecutorKind executor,
                                                   const RenderIntent& intent,
                                                   const RenderSceneAnalysis& sceneAnalysis) const {
    const SceneView sceneView = intent.defaultSceneView();
    std::set<RenderViewMode> seen;
    for (RenderViewMode viewMode : intent.exportedAOVs) {
      if (!seen.insert(viewMode).second) {
        continue;
      }
      addAuxiliaryAOVExport(plan, target, executor, viewMode, intent.defaultViewMode, sceneView,
                            intent, sceneAnalysis);
    }
  }

  RenderPlan
  RenderGraphCompiler::compileStencilCompositeView(const RenderTargetSpec& target,
                                                   const RenderIntent& intent,
                                                   const RenderSceneAnalysis& sceneAnalysis) const {
    const SceneView sceneView = intent.defaultSceneView();
    RenderPlan plan;

    const auto* rasterizerDefinition = renderExecutorDefinition(RenderExecutorKind::Rasterizer);
    if (!rasterizerDefinition) {
      throw std::runtime_error("stencil composite view requires a rasterizer beauty executor");
    }
    const auto* wireframeDefinition = renderExecutorDefinition(RenderExecutorKind::Wireframe);
    if (!wireframeDefinition) {
      throw std::runtime_error("stencil composite view requires a wireframe beauty executor");
    }

    RenderPassNode base = beautyPass(*rasterizerDefinition, sceneView, target, intent,
                                     sceneAnalysis, {"stencil_composite_base"});
    addRasterVisibilityInput(plan, target, base, sceneView, intent);
    RenderResourceDescriptor baseColor =
      target.colorResource("base_color", "Base color", RenderResourceLifetime::Transient);
    plan.addResourceProducer(base, baseColor);
    RenderResourceId baseCompositeInput = baseColor.id;
    if (beautyPassNeedsExplicitReadback(base)) {
      RenderResourceDescriptor baseReadback = target.colorResource(
        "base_readback_color", "Base readback color", RenderResourceLifetime::Transient);
      RenderPassNode readback =
        readbackPass("readback_base_color", "Read back base color", baseColor.id, baseReadback.id,
                     {"main", "stencil_composite"});
      plan.addResourceProducer(std::move(readback), baseReadback);
      baseCompositeInput = "base_readback_color";
    }

    plan.addResourceProducer(beautyPass(*wireframeDefinition, sceneView, target, intent,
                                        sceneAnalysis, {"stencil_composite_foreground"}),
                             target.colorResource("foreground_color", "Foreground color",
                                                  RenderResourceLifetime::Transient));

    const auto* stencilAOV = renderAOVDefinition(RenderViewMode::Stencil);
    if (!stencilAOV) {
      throw std::runtime_error("stencil composite view requires a stencil AOV definition");
    }

    RenderPassNode stencilProducer = aovProducerPass(
      *stencilAOV, RenderExecutorKind::Rasterizer, sceneView, true, target, intent, sceneAnalysis);
    addRasterVisibilityInput(plan, target, stencilProducer, sceneView, intent);
    RenderResourceDescriptor stencilResource =
      stencilAOV->resourceDescriptor(target, RenderResourceLifetime::Transient);
    RenderResourceId stencilCompositeInput = stencilResource.id;
    if (passNeedsExplicitReadback(stencilProducer)) {
      stencilProducer.id = "stencil_composite_mask";
      stencilProducer.name = "Stencil composite mask";
      stencilProducer.features.push_back("stencil_composite_mask");
      stencilResource.id = "stencil_composite_mask_source";
      stencilResource.name = "Stencil composite mask source";
      plan.addResourceProducer(std::move(stencilProducer), stencilResource);

      RenderResourceDescriptor stencilReadback =
        readbackResource(stencilResource, "stencil_composite_mask", "Stencil composite mask",
                         RenderResourceLifetime::Transient);
      RenderPassNode readback = readbackPass(
        "readback_stencil_composite_mask", "Read back stencil composite mask", stencilResource.id,
        stencilReadback.id, {"main", "stencil_composite"}, {"aov", stencilAOV->feature()});
      plan.addResourceProducer(std::move(readback), std::move(stencilReadback));
      stencilCompositeInput = "stencil_composite_mask";
    } else {
      plan.addResourceProducer(std::move(stencilProducer), std::move(stencilResource));
    }

    RenderPassNode composite;
    composite.id = "stencil_composite";
    composite.name = "Stencil composite";
    composite.kind = RenderPassKind::Composite;
    composite.executor = RenderExecutorKind::Composite;
    composite.features = {"main", "composite", "stencil_composite"};
    composite.addRead(std::move(baseCompositeInput));
    composite.addRead("foreground_color");
    composite.addRead(std::move(stencilCompositeInput));
    composite.addWrite("composited_color");
    composite.sceneView.selector = SceneSelector::all();
    composite.disabledBehavior = DisabledBehavior::SubstituteDefault;
    composite.concurrency = RenderConcurrencyLimit::serial();
    composite.canRunConcurrently = composite.concurrency.allowsParallelExecution();
    plan.addResourceProducer(std::move(composite),
                             target.colorResource("composited_color", "Composited color",
                                                  RenderResourceLifetime::Transient));

    plan.routeResourceThroughPass(
      "composited_color",
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported),
      tonemapPass("composited_color", "main_color"));

    addAuxiliaryAOVExports(plan, target, RenderExecutorKind::Rasterizer, intent, sceneAnalysis);
    return plan;
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& target,
                                          const RenderIntent& intent) const {
    return compile(target, intent, RenderSceneAnalysis::unknownScene());
  }

  RenderPlan RenderGraphCompiler::compile(const RenderTargetSpec& rawTarget,
                                          const RenderIntent& intent,
                                          const RenderSceneAnalysis& sceneAnalysis) const {
    return compileWithSubviewDepth(rawTarget, intent, sceneAnalysis, 0);
  }

  RenderPlan RenderGraphCompiler::compileWithSubviewDepth(const RenderTargetSpec& rawTarget,
                                                          const RenderIntent& intent,
                                                          const RenderSceneAnalysis& sceneAnalysis,
                                                          int renderToTextureDepth) const {
    const RenderTargetSpec target = rawTarget.normalized();
    RenderIntent frameIntent = intent.withWholeFrameOverridesApplied();
    sceneAnalysis.requireResolvableSelectors(frameIntent, "RenderGraphCompiler");
    addAutomaticFeatureSubviews(frameIntent, sceneAnalysis);
    if (renderToTextureDepth == 0) {
      validateSubviewReceivers(frameIntent, sceneAnalysis);
    }

    if (frameIntent.defaultViewMode == RenderViewMode::StencilComposite) {
      RenderPlan plan = compileStencilCompositeView(target, frameIntent, sceneAnalysis);
      const auto subviewOutputs =
        addSubviewBranches(plan, target, frameIntent, sceneAnalysis, renderToTextureDepth);
      addSubviewReceiverInputs(plan, subviewOutputs, sceneAnalysis);
      return plan;
    }

    const RenderExecutorKind executor = frameIntent.defaultExecutorKind();
    const auto* concreteExecutorDefinition = renderExecutorDefinition(executor);
    if (!concreteExecutorDefinition) {
      throw std::runtime_error("default render executor cannot produce a beauty pass");
    }
    const auto& preferredExecutorDefinition = renderExecutorDefinition(frameIntent.defaultExecutor);
    const RenderExecutorDefinition& beautyExecutorDefinition =
      preferredExecutorDefinition.kind() == executor ? preferredExecutorDefinition
                                                     : *concreteExecutorDefinition;

    if (const auto* aov = renderAOVDefinition(frameIntent.defaultViewMode)) {
      RenderPlan plan = this->aovViewPlan(target, executor, *aov, frameIntent.defaultSceneView(),
                                          frameIntent, sceneAnalysis);
      addAuxiliaryAOVExports(plan, target, executor, frameIntent, sceneAnalysis);
      const auto subviewOutputs =
        addSubviewBranches(plan, target, frameIntent, sceneAnalysis, renderToTextureDepth);
      addSubviewReceiverInputs(plan, subviewOutputs, sceneAnalysis);
      return plan;
    }

    RenderPlan plan;

    RenderResourceDescriptor beautyColor =
      target.colorResource("beauty_color", "Beauty color", RenderResourceLifetime::Transient);
    const bool usesPreviewShadows =
      sceneAnalysis.shouldCompileRasterPreviewShadows(executor, frameIntent);
    const RenderRasterShadowMode rasterShadowMode =
      frameIntent.engineOptions.rasterizer().shadowMode().value_or(
        RenderRasterShadowMode::ShadowMaps);
    const bool usesShadowMaps =
      usesPreviewShadows && rasterShadowMode == RenderRasterShadowMode::ShadowMaps;
    const bool usesRayTracedShadows =
      usesPreviewShadows && rasterShadowMode == RenderRasterShadowMode::RayTraced;

    RenderPassNode beauty = beautyPass(beautyExecutorDefinition, frameIntent.defaultSceneView(),
                                       target, frameIntent, sceneAnalysis);
    addRasterVisibilityInput(plan, target, beauty, frameIntent.defaultSceneView(), frameIntent);
    plan.addResourceProducer(beauty, beautyColor);

    RenderResourceId mainInputResource = beautyColor.id;
    if (beautyPassNeedsExplicitReadback(beauty)) {
      RenderResourceDescriptor readbackColor = target.colorResource(
        "beauty_readback_color", "Beauty readback color", RenderResourceLifetime::Transient);
      RenderPassNode readback = readbackPass(beautyColor.id, readbackColor.id);
      plan.addResourceProducer(std::move(readback), readbackColor);
      mainInputResource = "beauty_readback_color";
    }

    mainInputResource = addSelectorOverrideBranches(plan, target, std::move(mainInputResource),
                                                    frameIntent, sceneAnalysis);

    if (usesShadowMaps) {
      RenderPassNode shadows;
      shadows.id = "raster_preview_shadows";
      shadows.name = "Raster preview shadows";
      shadows.kind = RenderPassKind::Shadow;
      shadows.executor = RenderExecutorKind::Rasterizer;
      shadows.features = {"main", "preview_shadows", "shadow_maps", "rasterizer"};
      shadows.sceneView = frameIntent.defaultSceneView();
      const RasterShadowPassState shadowState =
        frameIntent.engineOptions.rasterizer().shadowPassState();
      shadowState.writeTo(shadows);
      shadows.disabledBehavior = DisabledBehavior::SubstituteDefault;
      shadows.concurrency = RenderConcurrencyLimit::serial();
      shadows.canRunConcurrently = shadows.concurrency.allowsParallelExecution();
      plan.connectProducerToConsumer(
        shadows,
        shadowState.shadows().resourceDescriptor("preview_shadow_map", "Raster preview shadow map"),
        beauty.id);
    }
    if (usesRayTracedShadows) {
      plan.addResourceProducer(hybridShadowMaskPass(frameIntent.defaultSceneView(), frameIntent),
                               hybridShadowMaskResource(target));
      mainInputResource = addHybridShadowComposite(plan, target, std::move(mainInputResource));
    }

    addAuxiliaryAOVExports(plan, target, executor, frameIntent, sceneAnalysis);
    const auto subviewOutputs =
      addSubviewBranches(plan, target, frameIntent, sceneAnalysis, renderToTextureDepth);
    addSubviewReceiverInputs(plan, subviewOutputs, sceneAnalysis);
    addAutomaticFeatureSubviewComposites(plan, target, frameIntent, sceneAnalysis,
                                         mainInputResource);

    RenderResourceDescriptor mainColor =
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported);
    plan.routeResourceThroughPass(mainInputResource, mainColor,
                                  tonemapPass(mainInputResource, "main_color"));

    if (frameIntent.usesGraphImagePostProcessAA()) {
      const auto* postAADefinition = postProcessAADefinition(frameIntent.postProcessAA);
      if (!postAADefinition) {
        throw std::runtime_error(
          "requested post-process AA mode cannot be compiled as a graph pass");
      }
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor postAAColor = target.colorResource(
        "post_aa_color", "Postprocess AA color", RenderResourceLifetime::Transient);
      RenderPassNode postAA;
      postAA.id = postAADefinition->passId();
      postAA.name = postAADefinition->passName();
      postAA.kind = RenderPassKind::PostProcess;
      postAA.executor = RenderExecutorKind::PostProcess;
      postAA.features = {"main", "postprocess", "post_aa", beautyExecutorDefinition.feature(),
                         postAADefinition->feature()};
      postAA.addRead(inputResource);
      postAA.addWrite(postAAColor.id);
      postAA.sceneView.selector = SceneSelector::all();
      postAA.state = postAADefinition->createState();
      postAA.disabledBehavior = DisabledBehavior::Passthrough;
      postAA.concurrency = RenderConcurrencyLimit::serial();
      postAA.canRunConcurrently = postAA.concurrency.allowsParallelExecution();
      plan.routeResourceThroughPass(inputResource, postAAColor, postAA);
    }

    if (frameIntent.enableWireframeOverlay) {
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor overlayColor =
        target.colorResource("overlay_color", "Overlay color", RenderResourceLifetime::Transient);
      RenderPassNode overlay;
      overlay.id = "wireframe_overlay";
      overlay.name = "Wireframe overlay";
      overlay.kind = RenderPassKind::Overlay;
      overlay.executor = RenderExecutorKind::Wireframe;
      overlay.features = {"main", "overlay", "wireframe", "wireframe_overlay"};
      overlay.addRead(inputResource);
      overlay.addWrite(overlayColor.id);
      overlay.sceneView = frameIntent.defaultSceneView();
      frameIntent.engineOptions.wireframe().passState().writeTo(overlay);
      overlay.disabledBehavior = DisabledBehavior::Passthrough;
      overlay.concurrency = RenderConcurrencyLimit::serial();
      overlay.canRunConcurrently = overlay.concurrency.allowsParallelExecution();
      plan.routeResourceThroughPass(inputResource, overlayColor, overlay);
    }

    if (frameIntent.enableCurveOverlay) {
      const RenderResourceId inputResource = tonemapInputResource(plan);
      RenderResourceDescriptor overlayColor = target.colorResource(
        "curve_overlay_color", "Curve overlay color", RenderResourceLifetime::Transient);
      RenderPassNode overlay;
      overlay.id = "curve_overlay";
      overlay.name = "Curve overlay";
      overlay.kind = RenderPassKind::Overlay;
      overlay.executor = RenderExecutorKind::Wireframe;
      overlay.features = {"main", "overlay", "curve", "curve_overlay", "wireframe"};
      overlay.addRead(inputResource);
      overlay.addWrite(overlayColor.id);
      overlay.sceneView = frameIntent.defaultSceneView();
      frameIntent.engineOptions.wireframe().passState().writeTo(overlay);
      overlay.disabledBehavior = DisabledBehavior::Passthrough;
      overlay.concurrency = RenderConcurrencyLimit::serial();
      overlay.canRunConcurrently = overlay.concurrency.allowsParallelExecution();
      plan.routeResourceThroughPass(inputResource, overlayColor, overlay);
    }

    return plan;
  }

  void
  RenderGraphCompiler::addAutomaticFeatureSubviews(RenderIntent& intent,
                                                   const RenderSceneAnalysis& sceneAnalysis) const {
    if (!intent.enableAutomaticFeatures) {
      return;
    }

    for (const auto& portal : sceneAnalysis.portalReceiverSurfaces()) {
      if (!portal.receiverVisibleInPrimaryView) {
        continue;
      }
      RenderSubviewIntent subview;
      subview.name = portalSubviewName(portal);
      subview.view.selector = SceneSelector::all();
      DerivedCameraRef derived;
      derived.kind = DerivedCameraRef::Kind::Portal;
      if (intent.defaultCamera && intent.defaultCamera->sceneCameraId) {
        derived.baseSceneCameraId = intent.defaultCamera->sceneCameraId;
      }
      derived.receiverTransform = portal.receiverTransform;
      derived.sourceTransform = portal.sourceTransform;
      derived.requiresReceiverClip = true;
      RenderCameraRef camera;
      camera.derived = derived;
      subview.view.camera = camera;
      intent.subviews.push_back(std::move(subview));
    }

    for (const auto& mirror : sceneAnalysis.planarMirrorSurfaces()) {
      if (!mirror.receiverVisibleInPrimaryView) {
        continue;
      }
      RenderSubviewIntent subview;
      subview.name = mirrorSubviewName(mirror);
      subview.view.selector = SceneSelector::all();
      DerivedCameraRef derived;
      derived.kind = DerivedCameraRef::Kind::PlanarMirror;
      if (intent.defaultCamera && intent.defaultCamera->sceneCameraId) {
        derived.baseSceneCameraId = intent.defaultCamera->sceneCameraId;
      }
      derived.mirrorPlanePoint = mirror.planePoint;
      derived.mirrorPlaneNormal = mirror.planeNormal;
      derived.requiresReceiverClip = true;
      RenderCameraRef camera;
      camera.derived = derived;
      subview.view.camera = camera;
      intent.subviews.push_back(std::move(subview));
    }
  }

  std::vector<RenderGraphCompiler::SubviewOutputBinding> RenderGraphCompiler::addSubviewBranches(
    RenderPlan& plan, const RenderTargetSpec& target, const RenderIntent& intent,
    const RenderSceneAnalysis& sceneAnalysis, int renderToTextureDepth) const {
    std::vector<SubviewOutputBinding> outputs;
    if (intent.subviews.empty()) {
      return outputs;
    }
    if (renderToTextureDepth >= intent.maxRenderToTextureRecursionDepth) {
      addSubviewRecursionLimitDiagnostics(plan, intent);
      return outputs;
    }

    std::set<std::string> usedPrefixes;
    for (std::size_t i = 0; i != intent.subviews.size(); ++i) {
      const RenderSubviewIntent& subview = intent.subviews[i];
      if (!subview.view.selector.selectsWholeFrame()) {
        throw std::runtime_error(
          "RenderGraphCompiler does not support selector-specific render-to-texture subviews yet "
          "(" +
          subview.view.selector.displayText() + ")");
      }

      RenderIntent subIntent = subviewRenderIntent(intent, subview);
      RenderPlan branch =
        compileWithSubviewDepth(target, subIntent, sceneAnalysis, renderToTextureDepth + 1);
      const std::string prefix = subviewPrefix(subview, i, usedPrefixes);
      const std::string displayName = subviewDisplayName(subview, i);
      const RenderFeatureKind feature = subviewFeature(prefix);
      RenderPlan prefixed = prefixedSubviewPlan(branch, prefix, displayName, subview.name, feature);

      SubviewOutputBinding binding;
      binding.name = subview.name;
      binding.colorResource = prefixedResourceId(prefix, "main_color");
      binding.depthResource = prefixedResourceId(prefix, "depth_aov");
      outputs.push_back(binding);
      const RenderPassId maskConsumerId =
        prefixed.passes().empty() ? "" : prefixed.passes().front().id;

      for (const auto& resource : prefixed.resources()) {
        plan.addResource(resource);
      }
      for (const auto& pass : prefixed.passes()) {
        plan.addPass(pass);
      }

      const auto derivedKind =
        subview.view.camera && subview.view.camera->derived &&
            subview.view.camera->derived->requiresReceiverClip
          ? std::optional<DerivedCameraRef::Kind>(subview.view.camera->derived->kind)
          : std::nullopt;
      if (!maskConsumerId.empty() && derivedKind) {
        for (const auto& portal : sceneAnalysis.portalReceiverSurfaces()) {
          if (*derivedKind == DerivedCameraRef::Kind::Portal &&
              portal.receiverVisibleInPrimaryView && subview.name == portalSubviewName(portal)) {
            addReceiverMaskDependency(plan, target, intent, portal, prefix, displayName,
                                      "portal_receiver", maskConsumerId);
            break;
          }
        }
        for (const auto& mirror : sceneAnalysis.planarMirrorSurfaces()) {
          if (*derivedKind == DerivedCameraRef::Kind::PlanarMirror &&
              mirror.receiverVisibleInPrimaryView && subview.name == mirrorSubviewName(mirror)) {
            addReceiverMaskDependency(plan, target, intent, mirror, prefix, displayName,
                                      "mirror_receiver", maskConsumerId);
            break;
          }
        }
      }
    }
    return outputs;
  }

  void RenderGraphCompiler::addSubviewReceiverInputs(
    RenderPlan& plan, const std::vector<SubviewOutputBinding>& subviewOutputs,
    const RenderSceneAnalysis& sceneAnalysis) const {
    if (subviewOutputs.empty() || sceneAnalysis.renderTextureSubviewReceivers().empty()) {
      return;
    }

    std::vector<RenderResourceId> resourcesToBind;
    for (const auto& output : subviewOutputs) {
      if (sceneAnalysis.renderTextureSubviewReceivers().find(output.name) ==
          sceneAnalysis.renderTextureSubviewReceivers().end()) {
        continue;
      }
      if (plan.findResource(output.colorResource)) {
        resourcesToBind.push_back(output.colorResource);
      }
      if (!output.depthResource.empty() && plan.findResource(output.depthResource)) {
        resourcesToBind.push_back(output.depthResource);
      }
    }
    if (resourcesToBind.empty()) {
      return;
    }

    std::vector<RenderPassId> receiverPasses;
    for (const auto& pass : plan.passes()) {
      if (passCanSampleSubviewTextures(pass)) {
        receiverPasses.push_back(pass.id);
      }
    }

    for (const auto& passId : receiverPasses) {
      for (const auto& resource : resourcesToBind) {
        plan.addReadToPass(passId, resource);
      }
    }
  }

  void
  RenderGraphCompiler::validateSubviewReceivers(const RenderIntent& intent,
                                                const RenderSceneAnalysis& sceneAnalysis) const {
    const auto& receivers = sceneAnalysis.renderTextureSubviewReceivers();
    if (receivers.empty()) {
      return;
    }

    std::map<std::string, std::size_t> subviewCounts;
    for (const auto& subview : intent.subviews) {
      if (!subview.name.empty()) {
        ++subviewCounts[subview.name];
      }
    }

    for (const auto& subview : intent.subviews) {
      if (!subview.name.empty() && subviewCounts[subview.name] > 1) {
        throw std::runtime_error("RenderGraphCompiler render-to-texture subview name '" +
                                 subview.name + "' is not unique");
      }
    }

    for (const auto& receiver : receivers) {
      const auto found = subviewCounts.find(receiver);
      if (found == subviewCounts.end()) {
        throw std::runtime_error("RenderGraphCompiler render-to-texture receiver references "
                                 "unknown subview '" +
                                 receiver + "'");
      }
    }
  }

  void RenderGraphCompiler::addAutomaticFeatureSubviewComposites(
    RenderPlan& plan, const RenderTargetSpec& target, const RenderIntent& intent,
    const RenderSceneAnalysis& sceneAnalysis, RenderResourceId& mainInputResource) const {
    if (intent.subviews.empty()) {
      return;
    }

    std::set<std::string> usedPrefixes;
    for (std::size_t i = 0; i != intent.subviews.size(); ++i) {
      const RenderSubviewIntent& subview = intent.subviews[i];
      const std::string prefix = subviewPrefix(subview, i, usedPrefixes);
      const std::string displayName = subviewDisplayName(subview, i);
      addAutomaticFeatureSubviewComposite(plan, target, subview, prefix, displayName, sceneAnalysis,
                                          mainInputResource);
    }
  }

  bool RenderGraphCompiler::addAutomaticFeatureSubviewComposite(
    RenderPlan& plan, const RenderTargetSpec& target, const RenderSubviewIntent& subview,
    const std::string& prefix, const std::string& displayName,
    const RenderSceneAnalysis& sceneAnalysis, RenderResourceId& mainInputResource) const {
    if (!subview.view.camera || !subview.view.camera->derived ||
        !subview.view.camera->derived->requiresReceiverClip) {
      return false;
    }

    RenderFeatureKind receiverFeature;
    if (subview.view.camera->derived->kind == DerivedCameraRef::Kind::Portal) {
      const auto found = std::find_if(
        sceneAnalysis.portalReceiverSurfaces().begin(),
        sceneAnalysis.portalReceiverSurfaces().end(), [&](const auto& portal) {
          return portal.receiverVisibleInPrimaryView && subview.name == portalSubviewName(portal);
        });
      if (found == sceneAnalysis.portalReceiverSurfaces().end()) {
        return false;
      }
      receiverFeature = "portal_receiver";
    } else {
      const auto found = std::find_if(
        sceneAnalysis.planarMirrorSurfaces().begin(), sceneAnalysis.planarMirrorSurfaces().end(),
        [&](const auto& mirror) {
          return mirror.receiverVisibleInPrimaryView && subview.name == mirrorSubviewName(mirror);
        });
      if (found == sceneAnalysis.planarMirrorSurfaces().end()) {
        return false;
      }
      receiverFeature = "mirror_receiver";
    }

    const RenderResourceId subviewColor = prefixedResourceId(prefix, "main_color");
    const RenderResourceId receiverMask = prefixedResourceId(prefix, "receiver_mask");
    if (!plan.findResource(subviewColor) || !plan.findResource(receiverMask)) {
      return false;
    }

    std::optional<RenderResourceId> baseDepth;
    std::optional<RenderResourceId> subviewDepth;
    const RenderResourceId subviewDepthCandidate = prefixedResourceId(prefix, "depth_aov");
    if (plan.findResource("depth_aov") && plan.findResource(subviewDepthCandidate)) {
      baseDepth = "depth_aov";
      subviewDepth = subviewDepthCandidate;
    }

    const RenderFeatureKind feature = subviewFeature(prefix);
    const RenderResourceId outputColor = prefixedResourceId(prefix, "composited_color");
    RenderResourceDescriptor output = target.colorResource(
      outputColor, displayName + " composited color", RenderResourceLifetime::Transient);
    output.addFeature("subview_composite");
    output.addFeature("render_to_texture");
    output.addFeature(feature);
    output.addFeature(receiverFeature);
    if (baseDepth && subviewDepth) {
      output.addFeature("depth_composite");
    }
    output.addFeature("stencil_composite");

    RenderPassNode composite =
      subviewCompositePass(prefix, displayName, feature, receiverFeature, mainInputResource,
                           subviewColor, baseDepth, subviewDepth, receiverMask, outputColor);
    plan.addResourceProducer(std::move(composite), std::move(output));
    mainInputResource = outputColor;
    return true;
  }

  RenderPassNode RenderGraphCompiler::subviewCompositePass(
    const std::string& prefix, const std::string& displayName,
    const RenderFeatureKind& subviewFeature, const RenderFeatureKind& receiverFeature,
    const RenderResourceId& baseColor, const RenderResourceId& subviewColor,
    const std::optional<RenderResourceId>& baseDepth,
    const std::optional<RenderResourceId>& subviewDepth, const RenderResourceId& receiverMask,
    const RenderResourceId& outputColor) const {
    RenderPassNode pass;
    pass.id = prefixedPassId(prefix, "composite");
    pass.name = displayName + " composite";
    pass.kind = RenderPassKind::Composite;
    pass.executor = RenderExecutorKind::Composite;
    pass.features = {"composite",         "subview_composite", "stencil_composite",
                     "render_to_texture", subviewFeature,      receiverFeature};
    if (baseDepth && subviewDepth) {
      pass.features.push_back("depth_composite");
    }
    pass.addRead(baseColor);
    pass.addRead(subviewColor);
    if (baseDepth && subviewDepth) {
      pass.addRead(*baseDepth);
      pass.addRead(*subviewDepth);
    }
    pass.addRead(receiverMask);
    pass.addWrite(outputColor);
    pass.sceneView.selector = SceneSelector::all();
    pass.disabledBehavior = DisabledBehavior::Passthrough;
    pass.canRunConcurrently = false;
    return pass;
  }

  void RenderGraphCompiler::addReceiverMaskDependency(
    RenderPlan& plan, const RenderTargetSpec& target, const RenderIntent& intent,
    const RenderSceneAnalysis::SceneSurfaceMarker& receiver, const std::string& prefix,
    const std::string& displayName, const RenderFeatureKind& receiverFeature,
    const RenderPassId& consumerPassId) const {
    const bool conservative = receiverMaskRequiresConservativeRasterState(intent);
    plan.connectProducerToConsumer(
      receiverMaskPass(intent, receiver, prefix, displayName, receiverFeature, conservative),
      receiverMaskResource(target, prefix, displayName, receiverFeature, conservative),
      consumerPassId);
  }

  RenderResourceDescriptor RenderGraphCompiler::receiverMaskResource(
    const RenderTargetSpec& target, const std::string& prefix, const std::string& displayName,
    const RenderFeatureKind& receiverFeature, bool conservative) const {
    RenderResourceDescriptor resource;
    resource.id = prefixedResourceId(prefix, "receiver_mask");
    resource.name = displayName + " receiver mask";
    resource.addFeature("receiver_mask");
    resource.addFeature("mask");
    resource.addFeature("stencil");
    resource.addFeature("rasterizer");
    resource.addFeature(receiverFeature);
    if (conservative) {
      resource.addFeature("conservative_receiver_mask");
    }
    resource.type = RenderResourceType::Stencil;
    resource.format = RenderResourceFormat::UInt8;
    resource.width = target.width;
    resource.height = target.height;
    resource.sampleCount = 1;
    resource.domain = RenderResourceDomain::CPU;
    resource.lifetime = RenderResourceLifetime::Transient;
    return resource;
  }

  RenderPassNode RenderGraphCompiler::receiverMaskPass(
    const RenderIntent& intent, const RenderSceneAnalysis::SceneSurfaceMarker& receiver,
    const std::string& prefix, const std::string& displayName,
    const RenderFeatureKind& receiverFeature, bool conservative) const {
    RenderPassNode pass;
    pass.id = prefixedPassId(prefix, "receiver_mask");
    pass.name = displayName + " receiver mask";
    pass.kind = RenderPassKind::AOV;
    pass.executor = RenderExecutorKind::Rasterizer;
    pass.features = {"receiver_mask", "mask", "stencil", "rasterizer", receiverFeature};
    if (conservative) {
      pass.features.push_back("conservative_receiver_mask");
    }
    pass.sceneView = intent.defaultSceneView();
    pass.sceneView.selector = conservative || receiver.surfaceId.empty()
                                ? SceneSelector::all()
                                : SceneSelector::objectId(receiver.surfaceId);
    pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
    pass.canRunConcurrently = false;

    RasterBeautyPassState state =
      intent.engineOptions.rasterizer().beautyPassState(1, RenderPostProcessAA::None, false, false);
    state.framebuffer().setColorWriteMask(0);
    state.framebuffer().configureStencilWritePass(0xff);
    state.writeTo(pass);
    return pass;
  }

  bool RenderGraphCompiler::receiverMaskRequiresConservativeRasterState(
    const RenderIntent& intent) const {
    const RasterBeautyPassState state =
      intent.engineOptions.rasterizer().beautyPassState(1, RenderPostProcessAA::None, false, false);
    return !state.framebuffer().supportsFrontToBackVisibilityOrdering();
  }

  void RenderGraphCompiler::addSubviewRecursionLimitDiagnostics(RenderPlan& plan,
                                                                const RenderIntent& intent) const {
    std::set<std::string> usedPrefixes;
    for (std::size_t i = 0; i != intent.subviews.size(); ++i) {
      plan.addPass(subviewRecursionLimitDiagnosticPass(
        intent.subviews[i], i, intent.maxRenderToTextureRecursionDepth, usedPrefixes));
    }
  }

  RenderPassNode RenderGraphCompiler::subviewRecursionLimitDiagnosticPass(
    const RenderSubviewIntent& subview, std::size_t index, int recursionLimit,
    std::set<std::string>& usedPrefixes) const {
    const std::string prefix = subviewPrefix(subview, index, usedPrefixes);
    const std::string displayName = subviewDisplayName(subview, index);
    const RenderFeatureKind feature = subviewFeature(prefix);

    std::ostringstream name;
    name << displayName << " truncated at render-to-texture recursion limit " << recursionLimit;

    RenderPassNode pass;
    pass.id = prefixedPassId(prefix, "recursion_limit");
    pass.name = name.str();
    pass.kind = RenderPassKind::Debug;
    pass.executor = RenderExecutorKind::PostProcess;
    pass.features = {"diagnostic",
                     "truncated",
                     "recursion_limit",
                     "render_to_texture_recursion_limit",
                     "subview",
                     "render_to_texture",
                     feature};
    pass.sceneView.selector = subview.view.selector;
    pass.sceneView.camera = subview.view.camera;
    pass.sceneView.shadingProfile = subview.view.shadingProfile;
    pass.disabledBehavior = DisabledBehavior::SubstituteDefault;
    pass.enabled = false;
    pass.canRunConcurrently = false;
    return pass;
  }

  RenderIntent RenderGraphCompiler::subviewRenderIntent(const RenderIntent& frameIntent,
                                                        const RenderSubviewIntent& subview) const {
    RenderIntent result;
    result.defaultExecutor = subview.view.executor.value_or(frameIntent.defaultExecutor);
    result.defaultViewMode = subview.view.viewMode.value_or(frameIntent.defaultViewMode);
    result.defaultShadingProfile =
      subview.view.shadingProfile.value_or(frameIntent.defaultShadingProfile);
    result.defaultCamera = subview.view.camera ? subview.view.camera : frameIntent.defaultCamera;
    result.enableAutomaticFeatures = frameIntent.enableAutomaticFeatures;
    result.enablePreviewShadows = frameIntent.enablePreviewShadows;
    result.postProcessAA = frameIntent.postProcessAA;
    result.engineOptions = subview.resolvedEngineOptions(frameIntent.engineOptions);
    result.maxRenderToTextureRecursionDepth = frameIntent.maxRenderToTextureRecursionDepth;
    if (result.defaultExecutorKind() == RenderExecutorKind::Rasterizer &&
        result.defaultViewMode != RenderViewMode::Depth) {
      result.requestExportedAOV(RenderViewMode::Depth);
    }
    return result;
  }

  RenderPlan RenderGraphCompiler::prefixedSubviewPlan(
    const RenderPlan& branch, const std::string& prefix, const std::string& displayName,
    const std::string& subviewName, const RenderFeatureKind& subviewFeature) const {
    RenderPlan result;
    for (auto resource : branch.resources()) {
      const RenderResourceId originalId = resource.id;
      resource.id = prefixedResourceId(prefix, resource.id);
      resource.name = displayName + " " + resource.name;
      resource.addFeature("subview");
      resource.addFeature(subviewFeature);
      resource.addFeature("render_to_texture");
      if (!subviewName.empty()) {
        resource.addFeature("subview_name:" + subviewName);
      }
      if (resource.lifetime == RenderResourceLifetime::Exported &&
          (originalId == "main_color" || resource.type == RenderResourceType::Depth)) {
        resource.addFeature("subview_output");
        if (originalId == "main_color") {
          resource.addFeature("subview_color_output");
        } else if (resource.type == RenderResourceType::Depth) {
          resource.addFeature("subview_depth_output");
        }
      }
      result.addResource(std::move(resource));
    }

    for (auto pass : branch.passes()) {
      pass.id = prefixedPassId(prefix, pass.id);
      pass.name = displayName + " " + pass.name;
      addFeature(pass, "subview");
      addFeature(pass, subviewFeature);
      addFeature(pass, "render_to_texture");
      if (!subviewName.empty()) {
        addFeature(pass, "subview_name:" + subviewName);
      }
      for (auto& read : pass.reads) {
        read.resource = prefixedResourceId(prefix, read.resource);
      }
      for (auto& write : pass.writes) {
        write.resource = prefixedResourceId(prefix, write.resource);
      }
      result.addPass(std::move(pass));
    }
    return result;
  }

  std::string RenderGraphCompiler::subviewPrefix(const RenderSubviewIntent& subview,
                                                 std::size_t index,
                                                 std::set<std::string>& usedPrefixes) const {
    const std::string sanitized = sanitizeSubviewIdentifier(subview.name);
    const std::string base =
      "subview_" + (sanitized.empty() ? std::to_string(index + 1) : sanitized);
    std::string prefix = base + "_";
    std::size_t suffix = 2;
    while (!usedPrefixes.insert(prefix).second) {
      prefix = base + "_" + std::to_string(suffix++) + "_";
    }
    return prefix;
  }

  RenderFeatureKind RenderGraphCompiler::subviewFeature(const std::string& prefix) const {
    std::string id = prefix;
    if (!id.empty() && id.back() == '_') {
      id.pop_back();
    }
    return "subview:" + id;
  }

  std::string RenderGraphCompiler::subviewDisplayName(const RenderSubviewIntent& subview,
                                                      std::size_t index) const {
    if (!subview.name.empty()) {
      return "Subview " + subview.name;
    }
    return "Subview " + std::to_string(index + 1);
  }

  std::string RenderGraphCompiler::sanitizeSubviewIdentifier(const std::string& name) const {
    std::string result;
    result.reserve(name.size());
    bool lastWasUnderscore = true;
    for (unsigned char ch : name) {
      if (std::isalnum(ch)) {
        result.push_back(static_cast<char>(std::tolower(ch)));
        lastWasUnderscore = false;
      } else if (!lastWasUnderscore) {
        result.push_back('_');
        lastWasUnderscore = true;
      }
    }
    if (!result.empty() && result.back() == '_') {
      result.pop_back();
    }
    return result;
  }

  RenderResourceId RenderGraphCompiler::prefixedResourceId(const std::string& prefix,
                                                           const RenderResourceId& id) const {
    return prefix + id;
  }

  RenderPassId RenderGraphCompiler::prefixedPassId(const std::string& prefix,
                                                   const RenderPassId& id) const {
    return prefix + id;
  }

  void RenderGraphCompiler::addFeature(RenderPassNode& pass, RenderFeatureKind feature) const {
    if (std::find(pass.features.begin(), pass.features.end(), feature) == pass.features.end()) {
      pass.features.push_back(std::move(feature));
    }
  }
}
