# Importer lifecycle

Scene importers translate external file formats into the editable world scene
graph. They are not renderers and they are not long-lived model objects: an
importer reads one source file, resolves any sidecar assets that source names,
returns a `Scene` or `Group` root, and reports diagnostics that explain anything
the conversion could not preserve exactly.

By the end of this chapter you should know:

- how `SceneImporter` exposes format names, extensions, options, and results,
- how sidecar assets are resolved relative to the imported source file,
- what belongs in diagnostics and source provenance,
- how new importer tests should use the shared fixture harness.

## Lifecycle

Every importer implements
[`SceneImporter`](../../../include/world/import/SceneImporter.h). The registry
creates importers by explicit format name or by filename extension, then calls
`importFile(filename, options)`. The implementation should keep all per-import
state on the stack or in local helper objects so repeated calls are independent.

The returned [`ImportResult`](../../../include/world/import/ImportResult.h)
owns the imported root. A full scene file can return `Scene`; an asset,
subassembly, or library part can return `Group`. Use `Group` when the source
format represents hierarchy that should be inserted into an existing scene.
Group `metadata` is the right place for source object IDs, collection names,
layer names, source categories, and other data useful to inspectors or later
exporters. It is intentionally opaque to the renderer.

When a standalone file import returns a `Group`, tools can still build a useful
scene around it. `SceneImporter::configureImportedScene(...)` is the importer
hook for that wrapper scene: add or tune the camera, lights, background, and
format-level orientation there instead of duplicating those choices in
rendercli, Modeler, or ad hoc import callers. Shared product-view defaults live
in [`ImportedSceneDefaults`](../../../include/world/import/ImportedSceneDefaults.h);
LDraw and OpenSCAD both use those defaults for white backgrounds, ambient fill,
directional light, and pinhole camera framing.

Parser-only format support can live below the scene-importer layer when the
result is useful data but not yet a renderable scene. The molecule parser is
one example: [`MoleculeParser`](../../../include/core/formats/molecule/MoleculeParser.h)
reads PDB `ATOM` / `HETATM` records and a supported PDBx/mmCIF `_atom_site`
subset into [`Molecule`](../../../include/core/formats/molecule/Molecule.h)
atoms, explicit PDB `CONECT` bonds, residues, chains, models, metadata, and
[`MoleculeSceneImporter`](../../../include/world/import/MoleculeSceneImporter.h)
then converts that structured data into a provenance-preserving `Group`
hierarchy with importer options for ball-and-stick, space-filling, and backbone
representations. Atom and bond materials can be colored by element, chain, or
residue category where the source data supports it. Keeping parser and scene
conversion separate lets the core coordinate reader stay independent of Qt while
the scene importer uses the same result contract as other formats.

## Options

[`ImportOptions`](../../../include/world/import/ImportOptions.h) stores
JSON-compatible values keyed by option name. The importer advertises its
expected controls through `optionSchema()`:

- `Boolean`, `Integer`, `Double`, and `String` cover direct scalar toggles.
- `FilePath` and `DirectoryPath` cover user-selected external references.
- `Choice` covers a fixed set of strings, with `choices` listing legal values.

Schema defaults document the importer's behavior, but importers should still
read each option with a fallback:

```cpp
const bool includeHidden = options.value("includeHidden", true).toBool();
```

That keeps direct unit tests, rendercli imports, and future GUI import dialogs
on the same path.

## Asset Resolution

External formats often reference files beside the source: textures, material
libraries, submodels, palettes, or binary buffers. Use
[`AssetResolver`](../../../include/core/formats/AssetResolver.h) for those
references. Pass the imported source filename as the current file:

```cpp
const core::AssetResolver resolver(searchRoots);
const auto asset = resolver.resolve(requestedPath, sourceFilename);
```

For relative paths, the resolver searches the source file's directory first,
then configured roots. The returned `ResolvedAsset::identity` is stable enough
for caches and provenance. If resolution fails, convert `AssetResolutionError`
into an import error diagnostic that includes the requested path and searched
roots instead of letting the exception escape without context.

## Generated Outputs

Some importers call external authoring tools before they can return renderer
geometry. [`OpenScadSceneImporter`](../../../include/world/import/OpenScadSceneImporter.h)
is the template: it asks
[`OpenScadCompiler`](../../../include/world/import/OpenScadCompiler.h) to locate
the optional `openscad` executable, compile the source into a generated STL
mesh, and cache that output by source contents plus import options. Missing
executables are reported as warnings with an empty imported group so a scene can
still load; failed tool runs or malformed generated meshes are errors.

Importers can expose source-specific controls through
`SceneImporter::editableSourceParameters(...)`. OpenSCAD uses this to scan
the Customizer block between `/*<!!start ...!!>*/` and `/*<!!end ...!!>*/`,
map `/* [Section] */` comments to property groups, copy the contiguous `//`
comments above an assignment into the property tooltip, and surface simple
numeric, boolean, string-choice, and vector-expression assignments on
[`SourceAsset`](../../../include/world/objects/SourceAsset.h) as dynamic
property-editor fields. Numeric trailing comments such as `// 0.01` become the
spinbox step and displayed precision. Editing one of those fields updates the
asset's `importOptions.define` object and rebuilds the generated children, so
the scene stores the source path plus overrides instead of baking the generated
mesh into JSON. `SourceAsset` also carries an optional material reference that
is applied as a whole-asset override during runtime scene conversion; generated
children stay transient and replaceable when source parameters change.
Standalone Open operations may call `SceneImporter::configureImportedScene(...)`
to apply product-view camera, lighting, and background defaults. Additive Modeler
imports call `SceneImporter::configureImportedRoot(...)` instead, so importer
root normalization such as OpenSCAD's Z-up orientation can still happen without
editing the destination scene.

Other importers preserve hierarchy before geometry support is complete.
[`GltfSceneImporter`](../../../include/world/import/GltfSceneImporter.h) maps
glTF scenes and nodes to `Group` roots, using node transforms for the local
group transform and attaching source IDs plus `ImportProvenance`. Its
`preserve_hierarchy` option keeps parent-child relationships by default; tests
can disable it to check the flattened global-transform path. Standalone glTF
opens use the shared product-view defaults: the imported root is normalized so
glTF's Y-up asset space appears upright in rendered images, then the scene gets
a white background, ambient fill, a default light, and a front-facing framed
pinhole camera. The shared pinhole framing path can raise zoom for unit-scale
assets, so small interchange models still fill the image instead of sitting in
the middle of a mostly empty frame.
When glTF animations contain simple node translation, rotation, or scale
channels, the importer preserves sampler/channel metadata on the node groups and
creates world timeline tracks for supported channels. Unsupported target paths
or interpolation modes continue as warnings so the imported hierarchy remains
inspectable. Mesh primitives compile to shared `MeshPrimitive` geometry, and
glTF PBR base-color factors or base-color textures become renderer
`MatteMaterial` diffuse textures. The
importer reports warnings when material features such as metallic/roughness,
alpha modes, double-sided rendering, or extensions cannot be represented by the
current renderer material model.

## Diagnostics

[`ImportDiagnostic`](../../../include/world/import/ImportDiagnostic.h) is for
actionable importer feedback. Use warnings when the import can continue after
downgrading or dropping data: unsupported annotations, approximated materials,
ignored custom properties, or hidden layers skipped by option. Use errors when
the result would be invalid: unreadable source files, missing required assets,
parse failures, invalid indices, or unsupported mandatory version features.

Line and column are one-based when the source format can provide them. Set them
to `-1` when a location is not meaningful. Always fill `source` with the file
that caused the diagnostic; for sidecar failures this may be the sidecar path,
not the top-level file.

Successful imports may still carry warnings. Failed imports should return an
`ImportResult::failed(...)` with diagnostics and source metadata, but no root.

## Provenance

Fill `ImportSourceMetadata` on every result:

- `importerName` should match `SceneImporter::name()`.
- `formatName` is a readable format label.
- `sourcePath` is the imported file path.
- `properties` carries format-specific provenance such as source version,
  asset identities, library roots, unit scale, or parser mode.

Object-level provenance belongs on imported `Group::metadata()` or on future
format-specific world objects. Keep it descriptive: use names such as
`sourceId`, `sourceLibrary`, `layerName`, and `assetIdentity` rather than
roadmap shorthand.

For ordered or time-sliced source data, prefer the shared group playback keys
over importer-specific names:

- `stepIndex` for build, assembly, or instruction order.
- `layerIndex` for source layers or frames when there is no separate step
  number.
- `startTime` and `endTime` for interval data. Either side may be omitted for
  an open-ended range.
- `label` for the user-facing display name of the step, layer, or interval.

[`StepVisibilityEvaluator`](../../../include/world/objects/StepVisibilityEvaluator.h)
uses those fields in that order: `stepIndex`, then `layerIndex`, then
`startTime` / `endTime`. Groups without playback metadata are static context.
The explicit `visible` flag and ancestor visibility still apply, so importers
should not duplicate visibility in metadata.

## Fixture Harness

Importer tests should put source files under
[`test/fixtures/importers/`](../../../test/fixtures/importers/) with sidecars
beside them. The shared helper
[`ImporterTestHelper`](../../../test/helpers/ImporterTestHelper.h) provides:

- `importerFixturePath(relative)` for locating fixture files from CTest,
- `expectDiagnostics(...)` for exact severity/message/source/line/column checks,
- `expectGroupTree(...)` for imported group names, visibility, metadata, and
  nested group shape.

The minimal self-test importer in
[`ImporterFixtureHarnessTest.cpp`](../../../test/unit/world/import/ImporterFixtureHarnessTest.cpp)
is the template to copy from when starting a new format. It reads a source file,
resolves a sidecar asset from the source directory, records provenance on the
root group, emits a warning diagnostic, and checks that an option changes the
imported group shape.

Generic group fixtures live under
[`test/fixtures/groups/`](../../../test/fixtures/groups/). Use
`nested_transforms_visibility.json` for hierarchy, transform, visibility, and
metadata round-trips. Use the `step_visibility_*.json` fixtures for playback
expectations: single-step filtering, cumulative filtering, active-step
highlighting, and previous-step ghosting. They intentionally avoid
source-format vocabulary so multiple importers can share them as expected
output.

## Render Smoke

For visible formats, add one render smoke after parser-level tests pass. Keep
the fixture tiny: one camera, one light, one visible object, and one intentionally
hidden or unsupported source feature if the importer handles those. Render
through `rendercli` when possible so the smoke covers registry lookup, option
passing, scene insertion, and the selected render engine. Use exact structure
assertions for importer contracts; use render smoke only to catch integration
mistakes that would make the imported scene blank or visibly wrong.

When the imported scene carries `stepIndex` / `layerIndex` / time-range group
metadata, add at least one playback smoke:

```bash
rendercli --engine raster --width 320 --height 240 --step 2 \
  imported_scene.json step_2.png

rendercli --engine raster --width 320 --height 240 --step 2 \
  --step_highlight --step_ghost_previous \
  imported_scene.json step_2_ghosted.png
```

`--step` renders a single active step. `--step_highlight` changes the active
step's material during conversion. `--step_ghost_previous` keeps earlier steps
visible with the ghost material. The flags do not edit the saved scene.

## Source anchors

<!-- source-anchors -->
- `include/world/import/SceneImporter.h`
- `include/world/import/ImportOptions.h`
- `include/world/import/ImportResult.h`
- `include/world/import/ImportDiagnostic.h`
- `include/world/import/ImportedSceneDefaults.h`
- `include/world/import/GltfSceneImporter.h`
- `include/world/animation/AnimationTrack.h`
- `include/world/animation/Timeline.h`
- `include/world/objects/Group.h`
- `include/world/objects/StepVisibilityEvaluator.h`
- `include/core/formats/molecule/Molecule.h`
- `include/core/formats/molecule/MoleculeParser.h`
- `include/world/import/MoleculeSceneImporter.h`
- `include/world/import/MoleculeSceneBuilder.h`
- `include/core/formats/AssetResolver.h`
- `include/core/formats/gltf/GltfAsset.h`
- `include/core/formats/gltf/GltfReader.h`
- `include/core/formats/stl/StlFile.h`
- `src/world/import/GltfSceneImporter.cpp`
- `src/world/animation/AnimationTrack.cpp`
- `src/world/animation/Timeline.cpp`
- `include/world/import/OpenScadCompiler.h`
- `include/world/import/OpenScadSceneImporter.h`
- `src/core/formats/AssetResolver.cpp`
- `src/core/formats/gltf/GltfReader.cpp`
- `src/core/formats/stl/StlFile.cpp`
- `src/world/import/OpenScadCompiler.cpp`
- `src/world/import/OpenScadSceneImporter.cpp`
- `src/world/import/ImportedSceneDefaults.cpp`
- `test/helpers/ImporterTestHelper.h`
- `test/helpers/ImporterTestHelper.cpp`
- `test/fixtures/groups/`
- `test/fixtures/importers/`
- `test/unit/world/import/ImporterFixtureHarnessTest.cpp`
- `test/unit/world/import/GltfSceneImporterTest.cpp`
<!-- /source-anchors -->
