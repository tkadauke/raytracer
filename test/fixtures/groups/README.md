These fixtures are intentionally small scene-graph examples for importer tests.

Each fixture is a complete `Scene` JSON file that can be loaded through
`Scene::load`. The group fixtures focus on hierarchy semantics rather than
photorealistic output:

- nested `Group` / `Collection` nodes,
- parent and child transforms that compose into runtime `Instance` transforms,
- inherited visibility for hidden groups and per-child visibility,
- opaque group `metadata` that round-trips without changing rendering.

Importers can reuse these files as expected-output scenes when verifying that
source collections, layers, assemblies, and named hierarchy nodes land in the
editable scene graph without being confused with render layers or AOV outputs.
