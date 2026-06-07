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

    RenderPassNode aovProducerPass(const RenderAOVDefinition& aov, RenderExecutorKind executor,
                                   const SceneView& sceneView, bool mainPass,
                                   const RenderTargetSpec& target, const RenderIntent& intent) {
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
      return pass;
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

  RenderPassNode
  RenderGraphCompiler::beautyPass(const RenderExecutorDefinition& executorDefinition,
                                  const SceneView& sceneView, const RenderTargetSpec& target,
                                  const RenderIntent& intent,
                                  std::vector<RenderFeatureKind> extraFeatures) const {
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
                                              overrides[i], i);
    }
    return currentBase;
  }

  RenderResourceId RenderGraphCompiler::addSelectorOverrideBranch(
    RenderPlan& plan, const RenderTargetSpec& target, RenderResourceId baseInputResource,
    const RenderIntent& frameIntent, const RenderViewOverride& viewOverride,
    std::size_t overrideIndex) const {
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
    RenderPassNode stencilProducer = aovProducerPass(*stencilAOV, RenderExecutorKind::Rasterizer,
                                                     sceneView, false, target, branchIntent);
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
        aovProducerPass(*aov, executor, sceneView, false, target, branchIntent);
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
      RenderPassNode foreground =
        beautyPass(beautyExecutorDefinition, sceneView, target, branchIntent,
                   {"selector_override"});
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
                                              const RenderIntent& intent) const {
    RenderPlan plan;

    const RenderResourceId aovId = aov.resourceId();
    RenderPassNode producer = aovProducerPass(aov, executor, sceneView, true, target, intent);
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

  void RenderGraphCompiler::addAuxiliaryAOVExport(RenderPlan& plan, const RenderTargetSpec& target,
                                                  RenderExecutorKind executor,
                                                  RenderViewMode viewMode,
                                                  RenderViewMode defaultViewMode,
                                                  const SceneView& sceneView,
                                                  const RenderIntent& intent) const {
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
      RenderPassNode producer = aovProducerPass(*aov, executor, sceneView, false, target, intent);
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
                                                   const RenderIntent& intent) const {
    const SceneView sceneView = intent.defaultSceneView();
    std::set<RenderViewMode> seen;
    for (RenderViewMode viewMode : intent.exportedAOVs) {
      if (!seen.insert(viewMode).second) {
        continue;
      }
      addAuxiliaryAOVExport(plan, target, executor, viewMode, intent.defaultViewMode, sceneView,
                            intent);
    }
  }

  RenderPlan RenderGraphCompiler::compileStencilCompositeView(const RenderTargetSpec& target,
                                                              const RenderIntent& intent) const {
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

    RenderPassNode base =
      beautyPass(*rasterizerDefinition, sceneView, target, intent, {"stencil_composite_base"});
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

    plan.addResourceProducer(
      beautyPass(*wireframeDefinition, sceneView, target, intent, {"stencil_composite_foreground"}),
      target.colorResource("foreground_color", "Foreground color",
                           RenderResourceLifetime::Transient));

    const auto* stencilAOV = renderAOVDefinition(RenderViewMode::Stencil);
    if (!stencilAOV) {
      throw std::runtime_error("stencil composite view requires a stencil AOV definition");
    }

    RenderPassNode stencilProducer =
      aovProducerPass(*stencilAOV, RenderExecutorKind::Rasterizer, sceneView, true, target, intent);
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

    addAuxiliaryAOVExports(plan, target, RenderExecutorKind::Rasterizer, intent);
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
    const RenderIntent frameIntent = intent.withWholeFrameOverridesApplied();
    sceneAnalysis.requireResolvableSelectors(frameIntent, "RenderGraphCompiler");
    if (renderToTextureDepth == 0) {
      validateSubviewReceivers(frameIntent, sceneAnalysis);
    }

    if (frameIntent.defaultViewMode == RenderViewMode::StencilComposite) {
      RenderPlan plan = compileStencilCompositeView(target, frameIntent);
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
      RenderPlan plan =
        this->aovViewPlan(target, executor, *aov, frameIntent.defaultSceneView(), frameIntent);
      addAuxiliaryAOVExports(plan, target, executor, frameIntent);
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

    RenderPassNode beauty =
      beautyPass(beautyExecutorDefinition, frameIntent.defaultSceneView(), target, frameIntent);
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

    RenderResourceDescriptor mainColor =
      target.colorResource("main_color", "Main color", RenderResourceLifetime::Exported);
    plan.routeResourceThroughPass(mainInputResource, mainColor,
                                  tonemapPass(mainInputResource, "main_color"));

    if (usesPreviewShadows) {
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

    addAuxiliaryAOVExports(plan, target, executor, frameIntent);
    const auto subviewOutputs =
      addSubviewBranches(plan, target, frameIntent, sceneAnalysis, renderToTextureDepth);
    addSubviewReceiverInputs(plan, subviewOutputs, sceneAnalysis);

    return plan;
  }

  std::vector<RenderGraphCompiler::SubviewOutputBinding>
  RenderGraphCompiler::addSubviewBranches(RenderPlan& plan, const RenderTargetSpec& target,
                                          const RenderIntent& intent,
                                          const RenderSceneAnalysis& sceneAnalysis,
                                          int renderToTextureDepth) const {
    std::vector<SubviewOutputBinding> outputs;
    if (intent.subviews.empty()) {
      return outputs;
    }
    if (renderToTextureDepth >= intent.maxRenderToTextureRecursionDepth) {
      std::ostringstream message;
      message << "RenderGraphCompiler render-to-texture recursion limit "
              << intent.maxRenderToTextureRecursionDepth << " reached";
      const std::string firstSubview =
        intent.subviews.front().name.empty() ? "unnamed subview" : intent.subviews.front().name;
      message << " (" << firstSubview;
      if (intent.subviews.size() > 1) {
        message << ", +" << (intent.subviews.size() - 1) << " more";
      }
      message << ")";
      throw std::runtime_error(message.str());
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
      RenderPlan prefixed =
        prefixedSubviewPlan(branch, prefix, displayName, subviewFeature(prefix));

      SubviewOutputBinding binding;
      binding.name = subview.name;
      binding.colorResource = prefixedResourceId(prefix, "main_color");
      binding.depthResource = prefixedResourceId(prefix, "depth_aov");
      outputs.push_back(binding);

      for (const auto& resource : prefixed.resources()) {
        plan.addResource(resource);
      }
      for (const auto& pass : prefixed.passes()) {
        plan.addPass(pass);
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

  void RenderGraphCompiler::validateSubviewReceivers(
    const RenderIntent& intent, const RenderSceneAnalysis& sceneAnalysis) const {
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

  RenderPlan
  RenderGraphCompiler::prefixedSubviewPlan(const RenderPlan& branch, const std::string& prefix,
                                           const std::string& displayName,
                                           const RenderFeatureKind& subviewFeature) const {
    RenderPlan result;
    for (auto resource : branch.resources()) {
      const RenderResourceId originalId = resource.id;
      resource.id = prefixedResourceId(prefix, resource.id);
      resource.name = displayName + " " + resource.name;
      resource.addFeature("subview");
      resource.addFeature(subviewFeature);
      resource.addFeature("render_to_texture");
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
