# Appendix C -- Source map

> **Generated** by `rake docs:textbook:source-map`. Do not edit by
> hand -- manual edits will be overwritten the next time the task
> runs.
>
> Reverse index from source files to chapters: lands a reader who
> opened a header / cpp on the chapters that talk about it.

## Index

| Source file | Chapters |
|---|---|
| `fuzz/` | [PLY parsing](../tools-and-io/ply-parsing.md) |
| `include/core/Buffer.h` | [Color and buffers](../foundations/color-and-buffers.md)<br>[Image buffers and pixel formats](../image-and-vision/image-buffers-and-pixel-formats.md) |
| `include/core/Color.h` | [Color and buffers](../foundations/color-and-buffers.md)<br>[Image buffers and pixel formats](../image-and-vision/image-buffers-and-pixel-formats.md) |
| `include/core/animation/AnimationTrack.h` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `include/core/animation/Timeline.h` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `include/core/color/sse3/` | [Color and buffers](../foundations/color-and-buffers.md) |
| `include/core/formats/AssetResolver.h` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `include/core/formats/ldraw/LDrawColorTable.h` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `include/core/formats/ldraw/LDrawCommand.h` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `include/core/formats/ldraw/LDrawFileResolver.h` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `include/core/formats/ldraw/LDrawGeometryCompiler.h` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `include/core/formats/ldraw/LDrawParser.h` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `include/core/geometry/AttributeColorMap.h` | [Tessellation](../rasterization/tessellation.md) |
| `include/core/geometry/Bresenham.h` | [Wireframe rendering](../rasterization/wireframe-rendering.md) |
| `include/core/geometry/Curve.h` | [Tessellation](../rasterization/tessellation.md) |
| `include/core/geometry/Mesh.h` | [Tessellation](../rasterization/tessellation.md) |
| `include/core/geometry/MeshAsset.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/core/geometry/Polyline.h` | [Tessellation](../rasterization/tessellation.md) |
| `include/core/geometry/Rasterize.h` | [Clipping, depth, stencil](../rasterization/clipping-depth-stencil.md)<br>[MSAA and attribute interpolation](../rasterization/msaa-and-attribute-interpolation.md)<br>[The rasterization pipeline](../rasterization/the-rasterization-pipeline.md) |
| `include/core/math/BoundingBox.h` | [Rays and geometry](../foundations/rays-and-geometry.md)<br>[Spatial acceleration](../scene-structure/spatial-acceleration.md) |
| `include/core/math/Constants.h` | [Numbers and vectors](../foundations/numbers-and-vectors.md) |
| `include/core/math/Cubic.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/core/math/GJKSimplex.h` | [Constructive solid geometry](../scene-structure/csg.md) |
| `include/core/math/HitPoint.h` | [Rays and geometry](../foundations/rays-and-geometry.md) |
| `include/core/math/HitPointInterval.h` | [Rays and geometry](../foundations/rays-and-geometry.md)<br>[Constructive solid geometry](../scene-structure/csg.md) |
| `include/core/math/Matrix.h` | [Matrices and transforms](../foundations/matrices-and-transforms.md)<br>[Instances and motion blur](../scene-structure/instances-and-motion-blur.md) |
| `include/core/math/Number.h` | [Numbers and vectors](../foundations/numbers-and-vectors.md) |
| `include/core/math/Polynomial.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/core/math/Quadric.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/core/math/Quartic.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/core/math/Quaternion.h` | [Matrices and transforms](../foundations/matrices-and-transforms.md) |
| `include/core/math/Range.h` | [Rays and geometry](../foundations/rays-and-geometry.md) |
| `include/core/math/Ray.h` | [Rays and geometry](../foundations/rays-and-geometry.md) |
| `include/core/math/Rect.h` | [Rays and geometry](../foundations/rays-and-geometry.md) |
| `include/core/math/Vector.h` | [Numbers and vectors](../foundations/numbers-and-vectors.md) |
| `include/core/math/interpolation/Interpolation.h` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `include/core/math/vector/sse3/` | [Numbers and vectors](../foundations/numbers-and-vectors.md) |
| `include/engine/graph/GraphRenderEngine.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/PostProcessPassState.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RasterPassState.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderExecutionContext.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderGraphCompiler.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderGraphExecutionObserver.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderGraphExecutionTrace.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderGraphRequest.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md)<br>[Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `include/engine/graph/RenderGraphTypes.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderPassPayload.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderPassState.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderPlan.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderResource.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderResourceStorage.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/RenderSceneAnalysis.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/graph/WireframePassState.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `include/engine/raster/Rasterizer.h` | [Clipping, depth, stencil](../rasterization/clipping-depth-stencil.md)<br>[MSAA and attribute interpolation](../rasterization/msaa-and-attribute-interpolation.md)<br>[The rasterization pipeline](../rasterization/the-rasterization-pipeline.md) |
| `include/engine/raster/detail/RasterMaterial.h` | [The rasterization pipeline](../rasterization/the-rasterization-pipeline.md) |
| `include/engine/raster/detail/RasterPass.h` | [Clipping, depth, stencil](../rasterization/clipping-depth-stencil.md) |
| `include/engine/raster/detail/RasterTriangleEmitter.h` | [The rasterization pipeline](../rasterization/the-rasterization-pipeline.md) |
| `include/engine/raytracer/Raytracer.h` | [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md) |
| `include/engine/wireframe/Wireframe.h` | [Wireframe rendering](../rasterization/wireframe-rendering.md) |
| `include/render/HomogeneousClipVolume.h` | [Clipping, depth, stencil](../rasterization/clipping-depth-stencil.md)<br>[The rasterization pipeline](../rasterization/the-rasterization-pipeline.md) |
| `include/render/RayCaster.h` | [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md) |
| `include/render/RenderEngine.h` | [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md) |
| `include/render/State.h` | [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md) |
| `include/render/TilePlan.h` | [The rasterization pipeline](../rasterization/the-rasterization-pipeline.md) |
| `include/render/brdf/BRDF.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/brdf/BTDF.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/brdf/GlossySpecular.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/brdf/Lambertian.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/brdf/PerfectSpecular.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/brdf/PerfectTransmitter.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/bsdf/BSDF.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/cameras/Camera.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/cameras/CameraFactory.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/cameras/EquirectangularCamera.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/cameras/FishEyeCamera.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/cameras/OrthographicCamera.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/cameras/PinholeCamera.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/cameras/SphericalCamera.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/cameras/ThinLensCamera.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/cameras/TiltShiftCamera.h` | [Cameras](../ray-rendering/cameras.md) |
| `include/render/lights/DirectionalLight.h` | [Lights and shading](../ray-rendering/lights-and-shading.md) |
| `include/render/lights/Light.h` | [Lights and shading](../ray-rendering/lights-and-shading.md) |
| `include/render/lights/PointLight.h` | [Lights and shading](../ray-rendering/lights-and-shading.md) |
| `include/render/materials/Material.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/materials/MatteMaterial.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/materials/PhongMaterial.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/materials/PortalMaterial.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/materials/ReflectiveMaterial.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/materials/TransparentMaterial.h` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `include/render/primitives/BVH.h` | [Spatial acceleration](../scene-structure/spatial-acceleration.md) |
| `include/render/primitives/Box.h` | [Tessellation](../rasterization/tessellation.md)<br>[Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/ClosedSolidUnion.h` | [Constructive solid geometry](../scene-structure/csg.md) |
| `include/render/primitives/Composite.h` | [Tessellation](../rasterization/tessellation.md)<br>[Constructive solid geometry](../scene-structure/csg.md) |
| `include/render/primitives/ConvexHull.h` | [Constructive solid geometry](../scene-structure/csg.md) |
| `include/render/primitives/ConvexOperation.h` | [Constructive solid geometry](../scene-structure/csg.md) |
| `include/render/primitives/Curve.h` | [Tessellation](../rasterization/tessellation.md) |
| `include/render/primitives/Difference.h` | [Constructive solid geometry](../scene-structure/csg.md) |
| `include/render/primitives/Disk.h` | [Tessellation](../rasterization/tessellation.md)<br>[Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/FlatMeshTriangle.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/Grid.h` | [Spatial acceleration](../scene-structure/spatial-acceleration.md) |
| `include/render/primitives/Instance.h` | [Matrices and transforms](../foundations/matrices-and-transforms.md)<br>[Tessellation](../rasterization/tessellation.md)<br>[Instances and motion blur](../scene-structure/instances-and-motion-blur.md) |
| `include/render/primitives/Intersection.h` | [Constructive solid geometry](../scene-structure/csg.md) |
| `include/render/primitives/MeshPrimitive.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/MeshTriangle.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/MinkowskiSum.h` | [Constructive solid geometry](../scene-structure/csg.md) |
| `include/render/primitives/OpenCylinder.h` | [Tessellation](../rasterization/tessellation.md)<br>[Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/Plane.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/Primitive.h` | [Tessellation](../rasterization/tessellation.md)<br>[Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/Rectangle.h` | [Tessellation](../rasterization/tessellation.md)<br>[Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/SmoothMeshTriangle.h` | [Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/Sphere.h` | [Tessellation](../rasterization/tessellation.md)<br>[Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/Torus.h` | [Tessellation](../rasterization/tessellation.md)<br>[Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/Triangle.h` | [Tessellation](../rasterization/tessellation.md)<br>[Primitives and intersection](../ray-rendering/primitives-and-intersection.md) |
| `include/render/primitives/Union.h` | [Constructive solid geometry](../scene-structure/csg.md) |
| `include/render/samplers/JitteredSampler.h` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `include/render/samplers/RandomSampler.h` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `include/render/samplers/RegularSampler.h` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `include/render/samplers/SampleStream.h` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `include/render/samplers/Sampler.h` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `include/render/samplers/SamplerFactory.h` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `include/render/textures/CheckerBoardTexture.h` | [Textures](../ray-rendering/textures.md) |
| `include/render/textures/ConstantColorTexture.h` | [Textures](../ray-rendering/textures.md) |
| `include/render/textures/ImageTexture.h` | [Textures](../ray-rendering/textures.md) |
| `include/render/textures/Texture.h` | [Textures](../ray-rendering/textures.md) |
| `include/render/textures/UVColorTexture.h` | [Textures](../ray-rendering/textures.md) |
| `include/render/textures/mappings/` | [Textures](../ray-rendering/textures.md) |
| `include/render/tonemap/AcesTonemap.h` | [Tone mapping](../ray-rendering/tone-mapping.md) |
| `include/render/tonemap/LinearTonemap.h` | [Tone mapping](../ray-rendering/tone-mapping.md) |
| `include/render/tonemap/ReinhardTonemap.h` | [Tone mapping](../ray-rendering/tone-mapping.md) |
| `include/render/tonemap/Tonemap.h` | [Tone mapping](../ray-rendering/tone-mapping.md) |
| `include/render/tonemap/TonemapFactory.h` | [Tone mapping](../ray-rendering/tone-mapping.md) |
| `include/render/viewplanes/PointInterlacedViewPlane.h` | [View planes](../scene-structure/view-planes.md) |
| `include/render/viewplanes/PointShuffledViewPlane.h` | [View planes](../scene-structure/view-planes.md) |
| `include/render/viewplanes/RowInterlacedViewPlane.h` | [View planes](../scene-structure/view-planes.md) |
| `include/render/viewplanes/RowShuffledViewPlane.h` | [View planes](../scene-structure/view-planes.md) |
| `include/render/viewplanes/TiledViewPlane.h` | [View planes](../scene-structure/view-planes.md) |
| `include/render/viewplanes/ViewPlane.h` | [View planes](../scene-structure/view-planes.md) |
| `include/render/viewplanes/ViewPlaneFactory.h` | [View planes](../scene-structure/view-planes.md) |
| `include/widgets/world/RenderGraphInspectorWidget.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md)<br>[Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `include/widgets/world/RenderGraphTracePreviewWidget.h` | [Render plans and resources](../render-graph/render-plans-and-resources.md)<br>[Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `include/world/animation/AnimationTrack.h` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `include/world/animation/Timeline.h` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `include/world/import/ImportDiagnostic.h` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `include/world/import/ImportOptions.h` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `include/world/import/ImportResult.h` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `include/world/import/LDrawSceneImporter.h` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `include/world/import/SceneImporter.h` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `include/world/objects/Group.h` | [Instances and motion blur](../scene-structure/instances-and-motion-blur.md)<br>[Importer lifecycle](../tools-and-io/importer-lifecycle.md) |
| `include/world/objects/Scene.h` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `include/world/objects/StepVisibilityEvaluator.h` | [Instances and motion blur](../scene-structure/instances-and-motion-blur.md)<br>[Importer lifecycle](../tools-and-io/importer-lifecycle.md) |
| `scenes/` | [Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `scenes/animation_frame_demo.json` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `scenes/render_graph_aov_demo.json` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `scenes/render_graph_stencil_composite_demo.json` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/core/formats/AssetResolver.cpp` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `src/core/formats/ldraw/LDrawColorTable.cpp` | [LDraw import](../tools-and-io/ldraw-import.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `src/core/formats/ldraw/LDrawCommand.cpp` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `src/core/formats/ldraw/LDrawFileResolver.cpp` | [LDraw import](../tools-and-io/ldraw-import.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `src/core/formats/ldraw/LDrawGeometryCompiler.cpp` | [LDraw import](../tools-and-io/ldraw-import.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `src/core/formats/ldraw/LDrawParser.cpp` | [LDraw import](../tools-and-io/ldraw-import.md)<br>[PLY parsing](../tools-and-io/ply-parsing.md) |
| `src/core/formats/ply/PlyElement.cpp` | [PLY parsing](../tools-and-io/ply-parsing.md) |
| `src/core/formats/ply/PlyFile.cpp` | [PLY parsing](../tools-and-io/ply-parsing.md) |
| `src/core/formats/ply/PlyProperty.cpp` | [PLY parsing](../tools-and-io/ply-parsing.md) |
| `src/engine/graph/GraphRenderEngine.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/PostProcessPassState.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RasterPassState.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderExecutionContext.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderGraphCompiler.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderGraphExecutionTrace.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderGraphRequest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderGraphTypes.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderPassPayloads.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderPassState.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderPlan.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderResource.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderResourceStorage.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/RenderSceneAnalysis.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/graph/WireframePassState.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `src/engine/raster/Rasterizer.cpp` | [Clipping, depth, stencil](../rasterization/clipping-depth-stencil.md)<br>[MSAA and attribute interpolation](../rasterization/msaa-and-attribute-interpolation.md)<br>[The rasterization pipeline](../rasterization/the-rasterization-pipeline.md) |
| `src/engine/raytracer/Raytracer.cpp` | [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md) |
| `src/engine/wireframe/Wireframe.cpp` | [Wireframe rendering](../rasterization/wireframe-rendering.md) |
| `src/modeler/` | [Render plans and resources](../render-graph/render-plans-and-resources.md)<br>[Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `src/modeler/MainWindow.cpp` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `src/widgets/world/RenderGraphInspectorWidget.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md)<br>[Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `src/widgets/world/RenderGraphTracePreviewWidget.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md)<br>[Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `src/world/import/LDrawSceneImporter.cpp` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `test/fixtures/groups/` | [Instances and motion blur](../scene-structure/instances-and-motion-blur.md)<br>[Importer lifecycle](../tools-and-io/importer-lifecycle.md) |
| `test/fixtures/importers/` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md) |
| `test/fixtures/ldraw/` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `test/functional/engine/wireframe/WireframeTest.cpp` | [Wireframe rendering](../rasterization/wireframe-rendering.md) |
| `test/functional/render/cameras/ThinLensCameraTest.cpp` | [Cameras](../ray-rendering/cameras.md) |
| `test/functional/render/lights/PointLightTest.cpp` | [Lights and shading](../ray-rendering/lights-and-shading.md) |
| `test/functional/render/materials/MatteMaterialTest.cpp` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `test/functional/render/materials/PhongMaterialTest.cpp` | [Materials and BRDFs](../ray-rendering/materials-and-brdfs.md) |
| `test/functional/render/samplers/SamplerDeterminismTest.cpp` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `test/functional/render/tonemap/TonemapMonotonicityTest.cpp` | [Tone mapping](../ray-rendering/tone-mapping.md) |
| `test/functional/steps/WireframeSteps.cpp` | [Wireframe rendering](../rasterization/wireframe-rendering.md) |
| `test/helpers/Blob.cpp` | [Blob analysis and silhouettes](../image-and-vision/blob-analysis-and-silhouettes.md) |
| `test/helpers/Blob.h` | [Blob analysis and silhouettes](../image-and-vision/blob-analysis-and-silhouettes.md)<br>[Image buffers and pixel formats](../image-and-vision/image-buffers-and-pixel-formats.md) |
| `test/helpers/ImporterTestHelper.cpp` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md) |
| `test/helpers/ImporterTestHelper.h` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md) |
| `test/helpers/ShapeClassifier.cpp` | [Shape classification](../image-and-vision/shape-classification.md) |
| `test/helpers/ShapeClassifier.h` | [Shape classification](../image-and-vision/shape-classification.md) |
| `test/helpers/Silhouette.cpp` | [Blob analysis and silhouettes](../image-and-vision/blob-analysis-and-silhouettes.md) |
| `test/helpers/Silhouette.h` | [Blob analysis and silhouettes](../image-and-vision/blob-analysis-and-silhouettes.md)<br>[Image buffers and pixel formats](../image-and-vision/image-buffers-and-pixel-formats.md)<br>[Shape classification](../image-and-vision/shape-classification.md) |
| `test/rendercli/FrameOptionTest.cmake` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `test/rendercli/RaytracerOptionTest.cmake` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `test/rendercli/RenderGraphOptionTest.cmake` | [Render plans and resources](../render-graph/render-plans-and-resources.md)<br>[Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `test/rendercli/StepOptionTest.cmake` | [Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `test/unit/core/animation/AnimationTrackTest.cpp` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `test/unit/core/formats/ldraw/LDrawColorTableTest.cpp` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `test/unit/core/formats/ldraw/LDrawFileResolverTest.cpp` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `test/unit/core/formats/ldraw/LDrawGeometryCompilerTest.cpp` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `test/unit/core/formats/ply/PlyFileTest.cpp` | [PLY parsing](../tools-and-io/ply-parsing.md) |
| `test/unit/core/math/interpolation/InterpolationTest.cpp` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `test/unit/engine/graph/GraphRenderEngineTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/engine/graph/PostProcessPassStateTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/engine/graph/RasterPassStateTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/engine/graph/RenderExecutionContextTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/engine/graph/RenderGraphCompilerTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/engine/graph/RenderPlanTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/engine/graph/RenderResourceStorageTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/engine/graph/WireframePassStateTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/render/WireframeTest.cpp` | [Wireframe rendering](../rasterization/wireframe-rendering.md) |
| `test/unit/render/primitives/BVHPerformanceTest.cpp` | [Spatial acceleration](../scene-structure/spatial-acceleration.md) |
| `test/unit/render/samplers/JitteredSamplerTest.cpp` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `test/unit/render/samplers/RandomSamplerTest.cpp` | [Sampling and anti-aliasing](../ray-rendering/sampling-and-anti-aliasing.md) |
| `test/unit/widgets/world/RenderGraphInspectorWidgetTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/widgets/world/RenderGraphTracePreviewWidgetTest.cpp` | [Render plans and resources](../render-graph/render-plans-and-resources.md) |
| `test/unit/world/animation/AnimationTrackTest.cpp` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `test/unit/world/animation/TimelineTest.cpp` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `test/unit/world/import/ImporterFixtureHarnessTest.cpp` | [Importer lifecycle](../tools-and-io/importer-lifecycle.md) |
| `test/unit/world/objects/LDrawSceneImporterTest.cpp` | [LDraw import](../tools-and-io/ldraw-import.md) |
| `test/unit/world/objects/SceneTest.cpp` | [Timelines and interpolation](../animation/timelines-and-interpolation.md) |
| `tools/rendercli/` | [Tools and the Modeler](../tools-and-io/tools-and-modeler.md) |
| `tools/rendercli/rendercli.cpp` | [Timelines and interpolation](../animation/timelines-and-interpolation.md)<br>[Render plans and resources](../render-graph/render-plans-and-resources.md) |

## See also

- [Top-level TOC](../README.md)
- [A. Glossary](a-glossary.md)
- [B. Bibliography](b-bibliography.md)
