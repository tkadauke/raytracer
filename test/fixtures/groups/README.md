These fixtures are intentionally small scene-graph examples for importer tests.

Each fixture is a complete `Scene` JSON file that can be loaded through
`Scene::load`. The group fixtures focus on hierarchy semantics rather than
photorealistic output:

- nested `Group` / `Collection` nodes,
- parent and child transforms that compose into runtime `Instance` transforms,
- inherited visibility for hidden groups and per-child visibility,
- opaque group `metadata` that round-trips without changing rendering.
- generic step/timeline metadata that drives playback visibility.

Importers can reuse these files as expected-output scenes when verifying that
source collections, layers, assemblies, and named hierarchy nodes land in the
editable scene graph without being confused with render layers or AOV outputs.

## Step visibility fixtures

The `step_visibility_*.json` fixtures are domain-neutral playback examples.
They use only `Group` / `Collection` nodes and generic metadata:

- `step_visibility_single_step.json` — expected with
  `StepVisibilitySelection::onlyStep(2)`: static context plus step 2.
- `step_visibility_cumulative.json` — expected with
  `StepVisibilitySelection::cumulativeThrough(2)`: static context plus steps
  1 and 2.
- `step_visibility_highlighted.json` — expected with active-step styling:
  step 2 receives the active visual role, previous/future steps are hidden.
- `step_visibility_ghosted.json` — expected with active-step highlighting and
  previous-step ghosting: step 1 is previous, step 2 is active, step 3 is
  hidden.

These files avoid source-format terms such as "brick", "slice", or "toolpath"
so importer tests for unrelated formats can reuse the same expected hierarchy.
