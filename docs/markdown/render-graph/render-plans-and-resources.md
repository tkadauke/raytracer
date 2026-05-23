# Render plans and resources

A renderer usually starts as one function that fills one image. That is easy to
understand, but it hides the intermediate images and decisions that make a
frame. The render graph layer exposes those intermediate products as named
resources and exposes the work as named pass declarations.

In this codebase, the graph layer is centered on
[`RenderPlan`](../../../include/engine/graph/RenderPlan.h). A plan is a
deterministic list of resource descriptors plus a deterministic list of pass
nodes. It can be validated, exported as text, exported as DOT, exported as
JSON, and copied with user-specified disable overrides.

## <a id="the-vocabulary-lives-in-one-place"></a>The vocabulary lives in one place
[`RenderGraphTypes.h`](../../../include/engine/graph/RenderGraphTypes.h)
contains the shared data types used by the graph module. The small type aliases
make ids explicit:

```cpp
using RenderPassId = std::string;
using RenderResourceId = std::string;
using RenderFeatureKind = std::string;
```

A pass id names a unit of work, a resource id names a produced or consumed data
product, and a feature kind gives users a higher-level switch such as
`"shadow_maps"` or `"main"`.

The same header defines the enum classes used by plan declarations:

- `RenderExecutorPreference` names a user's broad engine preference:
  raytracer, rasterizer, or wireframe.
- `RenderExecutorKind` names the executor required by a compiled pass:
  raytracer, rasterizer, wireframe, composite, or postprocess.
- `RenderPassKind` groups passes as beauty, shadow, overlay, composite,
  tonemap, postprocess, AOV, debug, or custom.
- `RenderResourceType` classifies image-like graph products such as color,
  depth, stencil, object id, material id, normals, world positions, motion
  vectors, shadow maps, shadow masks, and custom textures.

Each enum has a `toString(...)` helper implemented next to the type
definitions in
[`RenderGraphTypes.cpp`](../../../src/engine/graph/RenderGraphTypes.cpp).
Exports use those helpers, so text, DOT, and JSON dumps use the same spelling.

## <a id="intent-describes-what-the-user-asked-for"></a>Intent describes what the user asked for
`RenderIntent` is the user-facing request. It says what executor and view mode
the frame should use by default, names the default shading profile, optionally
names a default camera, and stores per-selection overrides.

Scene selections use `SceneSelector`:

```cpp
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
};
```

That gives the graph layer a stable way to say "all objects", "this object",
"this layer", or "objects with this material role" without tying the core plan
model to Qt widgets or JSON parsing.

`RenderViewOverride` combines a selector with optional replacements for
executor, view mode, shading profile, and camera. The result is a layered
request: one default frame intent plus targeted overrides for specific parts
of the scene.

## <a id="resources-are-descriptors-not-buffers"></a>Resources are descriptors, not buffers
A render resource is declared with `RenderResourceDescriptor`:

```cpp
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
```

The descriptor records what the resource is. It does not own the memory behind
that resource. That separation is important: a plan can be printed, validated,
or displayed without allocating every image it mentions.

Resource domains are `CPU` and `GPU`. `CPU` resources can be allocated by
`RenderResourceStorage`; `GPU` resources are descriptors without CPU buffers in
that storage object. Resource lifetimes are:

- `Transient` -- produced inside the plan and consumed by other passes.
- `Imported` -- available before the plan starts.
- `Exported` -- a result the caller can take after the plan.
- `History` -- a carried-over resource from an earlier frame.
- `PersistentCache` -- a retained cache resource.

Validation treats imported, history, and persistent-cache resources as
externally available. A transient or exported resource that is read must have a
producer in the plan.

## <a id="pass-nodes-declare-reads-and-writes"></a>Pass nodes declare reads and writes
`RenderPassNode` is the declarative node in the graph:

```cpp
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
```

The central fields are `reads` and `writes`. A tonemap pass, for example, can
read `main_color` and write `display_color`. A shadow pass can write
`shadow_mask`, and a beauty pass can read that mask before writing
`main_color`.

The node also carries a `DisabledBehavior` value:

- `Error` means the pass is required.
- `CullDependents` names a pass whose consumers cannot use its outputs when it
  is disabled.
- `SubstituteDefault` means the pass still satisfies consumers by substituting
  a default output.
- `Passthrough` records pass-through disable intent for the node.

Validation gives producer semantics to `SubstituteDefault`: a disabled producer
with that behavior can still satisfy consumers. A disabled producer with any
other behavior triggers `DisabledDependency` when another enabled pass reads
its output.

The pass declaration is separate from execution code. Executor-specific work
lives behind
[`RenderPassPayload`](../../../include/engine/graph/RenderPassPayload.h), whose
interface is a virtual `execute(RenderExecutionContext&)` method. The node
describes the graph; the payload performs the pass.

## <a id="validation-catches-graph-mistakes"></a>Validation catches graph mistakes
`RenderPlan::validate()` walks the resource list and pass list and returns a
`RenderPlanValidation`. Each issue is a `RenderPlanValidationError` with a
machine-readable code, a human-readable message, and the relevant pass/resource
ids.

Validation checks:

- empty pass ids and resource ids,
- duplicate pass ids and resource ids,
- reads or writes of unknown resources,
- multiple passes writing the same resource,
- reads of transient/exported resources that have no producer,
- disabled required passes,
- reads from disabled producers that do not substitute a default output,
- negative dimensions or non-positive sample counts,
- dependency cycles.

The dependency graph comes from resource flow. If pass A writes a resource
that pass B reads, B depends on A. A cycle exists when the dependency walk can
return to a pass already on the active stack. A single pass reading and writing
the same resource is also reported as a cycle.

## <a id="exports-make-the-plan-inspectable"></a>Exports make the plan inspectable
`RenderPlan` has three export surfaces:

- `toText()` produces a compact human-readable dump.
- `toDot()` produces a Graphviz DOT graph.
- `toJson()` produces a structured `QJsonObject`.

`rendercli` exposes the same formats through `--render_graph_only` and
`--render_graph_format text|dot|json`. With `--render_graph_out`, the CLI can
save the compiled graph alongside a graph-backed render; without an output file,
graph-only mode writes the graph to standard output.

JSON exports are also accepted as input through `--render_graph_in`. That makes
the graph a real intermediate artifact: compile a plan, edit or inspect the
JSON, then replay the plan as DOT/text or render through it. Disable filters are
applied after the JSON is loaded, so a saved plan can still be tested with
`--disable_pass`, `--disable_pass_kind`, `--disable_executor`, and
`--disable_feature`. When a loaded graph is rendered, `rendercli` uses the
exported color resource dimensions unless `--width` or `--height` explicitly
request matching values.

Before compilation, `rendercli` can also override the default graph intent:
`--render_graph_executor` selects the default executor and
`--render_graph_view` selects the structural view mode. The current compiler
uses those fields to choose the initial whole-frame beauty executor; selector
specific overrides will matter once compilation can produce multi-selection
plans.

A two-pass plan might read textually as:

```text
RenderPlan
Resources:
- main_color (color, rgb_double, cpu, transient, 640x360, samples=1)
- display_color (color, rgb_double, cpu, exported, 640x360, samples=1)
Passes:
- main [beauty/rasterizer] enabled
  writes: main_color
- tonemap [tonemap/postprocess] enabled
  reads: main_color
  writes: display_color
```

The DOT export uses resource nodes and pass nodes, with arrows from resources
to reader passes and from writer passes to resources. The JSON export carries
the same ids, enum strings, resource dimensions, pass features, reads, writes,
scene selector, disabled behavior, and scheduling flags.

The first graph-backed render that exists today is deliberately small: a
whole-frame raytraced beauty pass writes the exported color resource. The DOT
artifact checked into this chapter is the same shape emitted by `rendercli` for
`scenes/dice.json` when asked to compile, but not render, the graph.

![Raytraced beauty render graph](../../images/render_graph_raytrace_beauty.svg)

DOT source: [render_graph_raytrace_beauty.dot](render_graph_raytrace_beauty.dot)

## <a id="overrides-disable-by-id-kind-executor-or-feature"></a>Overrides disable by id, kind, executor, or feature
`RenderGraphOverrides` stores four sets:

```cpp
struct RenderGraphOverrides {
  std::set<RenderPassId> disabledPasses;
  std::set<RenderPassKind> disabledPassKinds;
  std::set<RenderExecutorKind> disabledExecutors;
  std::set<RenderFeatureKind> disabledFeatures;
};
```

`RenderPlan::withOverrides()` returns a copy of the plan. For each pass, it
sets `enabled = false` if the pass id, pass kind, executor kind, or any feature
matches one of the disabled sets. The original plan stays unchanged, so a UI or
CLI can try different disable combinations and validate each result.

The tests cover all four paths in one plan: disabling a pass by id, disabling
shadow passes by kind, disabling wireframe passes by executor, and disabling a
named `"main"` feature.

## <a id="storage-owns-cpu-buffers"></a>Storage owns CPU buffers
[`RenderResourceStorage`](../../../include/engine/graph/RenderResourceStorage.h)
is the allocation side of the descriptor model. `allocate()` receives a list of
resource descriptors, records every descriptor, and creates CPU buffers for
supported CPU image resources with positive width and height.

The storage creates execution-time
[`RenderResource`](../../../include/engine/graph/RenderResource.h) objects.
The descriptor remains the serializable plan data; the resource object owns the
runtime buffer and answers capability questions such as `colorBacked()`.

The first CPU storage slice maps resource descriptors to these resource
classes:

| Resource types | CPU buffer |
|---|---|
| `Color`, `Normal`, `WorldPosition`, `MotionVector`, `ShadowMask`, `CustomTexture` | `Buffer<Colord>` |
| `Depth`, `ShadowMap` | `Buffer<double>` |
| `Stencil` | `Buffer<std::uint8_t>` |
| `ObjectId`, `MaterialId` | `Buffer<std::uint32_t>` |

Descriptors with `RenderResourceDomain::GPU` are still recorded, but
`hasBuffer(id)` is false for them in CPU storage. Typed accessors such as
`color(id)`, `depth(id)`, `stencil(id)`, and `objectId(id)` throw
`std::out_of_range` when the id is missing or when the descriptor does not have
that concrete CPU buffer. Execution code can also ask for the resource object
directly through `resource(id)` and use virtual capabilities instead of
switching on `RenderResourceType`.

## <a id="the-first-compiler-emits-a-beauty-pass"></a>The first compiler emits a beauty pass
[`RenderGraphCompiler`](../../../include/engine/graph/RenderGraphCompiler.h)
turns `RenderIntent` into a concrete `RenderPlan`. The first compiler slice
targets one whole-frame beauty pass. It declares one exported CPU color
resource named `main_color`, then adds one pass that writes it:

- `raytrace_beauty` for the default raytracer executor,
- `raster_beauty` when the intent prefers the rasterizer,
- `wireframe_beauty` when the intent requests a wireframe view.

`RenderTargetSpec` supplies the framebuffer width, height, and sample count for
the resource descriptor. Compilation does not allocate buffers and does not
render; it only produces the inspectable plan.

## <a id="inspecting-plans-in-modeler"></a>Inspecting plans in Modeler
The `Modeler` Render Graph dock is the GUI counterpart to rendercli's
graph-only dump. The dock compiles the current live-preview intent and target
size into a `RenderPlan`, then shows the result as two tables:

- the Passes table lists each pass id, pass kind, executor, read resources,
  written resources, and disabled behavior;
- the Resources table lists each resource id, type, format, domain, lifetime,
  dimensions, and sample count.

The checkbox in each pass row builds a `RenderGraphOverrides` value for that
pass id. The dock applies those overrides to the compiled plan and runs
`RenderPlan::validate()` immediately. Disabling the first required beauty pass
therefore reports an invalid plan because that pass has
`DisabledBehavior::Error`.

## <a id="the-first-graph-engine-executes-one-pass"></a>The first graph engine executes simple plans
[`GraphRenderEngine`](../../../include/engine/graph/GraphRenderEngine.h) is a
`RenderEngine` facade over the graph path. It can compile from its current
intent or execute a caller-provided plan. The compiler still emits only one
whole-frame beauty pass, but the engine can execute a small serial color
resource chain:

- enabled `Beauty` passes backed by `Raytracer`, `Rasterizer`, or `Wireframe`;
- enabled `Tonemap` passes backed by the `PostProcess` executor;
- disabled passes with `SubstituteDefault`, which clear their outputs to a
  meaningful default;
- disabled color passes with `Passthrough`, which copy their input color
  resource to their output color resources.

The graph engine writes pass results into `RenderResourceStorage` and then
copies the first exported color resource into the caller's output buffer.
`lastPlan()` remains available after rendering, so tools can render and then
inspect the exact graph shape that produced the image.

Composite passes, arbitrary postprocess effects, graph scheduling, and history
resources are not executed by this first slice.

## <a id="a-small-plan-by-hand"></a>A small plan by hand
The unit tests build plans directly. A simple producer-consumer graph looks
like this:

```cpp
RenderPlan plan;
plan.addResource(colorResource("main_color"));
plan.addResource(colorResource("display_color",
                               RenderResourceLifetime::Exported));

auto main = pass("main");
main.writes.push_back({"main_color"});
plan.addPass(main);

auto tonemap = pass("tonemap", RenderPassKind::Tonemap);
tonemap.reads.push_back({"main_color"});
tonemap.writes.push_back({"display_color"});
plan.addPass(tonemap);

EXPECT_TRUE(plan.validate().valid());
```

The important shape is not the helper syntax. It is the resource edge:
`main` writes `main_color`, `tonemap` reads `main_color`, and `tonemap` writes
the exported `display_color`.

Changing that one edge changes validation. If `tonemap` reads a transient
`main_color` without a writer, validation reports `MissingProducer`. If two
passes both write `main_color`, validation reports `DuplicateWriter`. If pass
A reads B's output while B reads A's output, validation reports `Cycle`.

## <a id="exercises"></a>Exercises
1. In a plan with `shadow`, `main`, and `tonemap` passes, which resources make
   `main` depend on `shadow`, and which resource makes `tonemap` depend on
   `main`?
2. Why does an imported resource not need a producer in the plan?
3. What validation error should a pass get if it reads a resource written by a
   disabled pass whose `DisabledBehavior` is `CullDependents`?
4. Build a three-pass plan by hand: one pass writes `object_id`, one pass
   writes `main_color`, and one postprocess pass reads both resources.

## See also

- Volume index: [Render graph](README.md)
- Previous volume:
  [Tools & I/O](../tools-and-io/README.md)
- Next volume:
  [Animation](../animation/README.md)
- Rasterizer resources:
  [Clipping, depth, stencil](../rasterization/clipping-depth-stencil.md)
- Tone mapping as a final image pass:
  [Tone mapping](../ray-rendering/tone-mapping.md)

## Source anchors

<!-- source-anchors -->
- `include/engine/graph/RenderGraphTypes.h`
- `include/engine/graph/RenderGraphCompiler.h`
- `include/engine/graph/RenderPlan.h`
- `include/engine/graph/RenderPassPayload.h`
- `include/engine/graph/RenderResource.h`
- `include/engine/graph/RenderResourceStorage.h`
- `include/engine/graph/GraphRenderEngine.h`
- `include/widgets/world/RenderGraphInspectorWidget.h`
- `src/engine/graph/RenderGraphCompiler.cpp`
- `src/engine/graph/RenderGraphTypes.cpp`
- `src/engine/graph/GraphRenderEngine.cpp`
- `src/engine/graph/RenderPlan.cpp`
- `src/engine/graph/RenderResource.cpp`
- `src/engine/graph/RenderResourceStorage.cpp`
- `src/widgets/world/RenderGraphInspectorWidget.cpp`
- `test/unit/engine/graph/RenderGraphCompilerTest.cpp`
- `test/unit/engine/graph/GraphRenderEngineTest.cpp`
- `test/unit/engine/graph/RenderPlanTest.cpp`
- `test/unit/engine/graph/RenderResourceStorageTest.cpp`
- `test/unit/widgets/world/RenderGraphInspectorWidgetTest.cpp`
- `tools/rendercli/rendercli.cpp`
- `test/rendercli/RenderGraphOptionTest.cmake`
<!-- /source-anchors -->
