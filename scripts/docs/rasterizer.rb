# Doc-render driver for `engine::raster::Rasterizer`.
#
# Hero image: a sphere at default LOD = 0. Same scene-shape as
# `wireframe_engine.rb` so the wireframe-vs-rasterizer comparison
# is direct: same primitive, same camera, just a filled rendering
# instead of edges.
#
# Property sweep: same sphere across LOD 0..4. Higher LOD =
# denser triangulation = smoother shaded silhouette (the
# per-face hash colours blur out as triangles shrink past the
# pixel size).

class_doc(engine: "raster", width: 320, height: 240) do
  name "rasterizer_engine"

  sunlight
  pinhole_camera :position => [0, 0, -3], :zoom => 1.4
  sphere
end

property_doc(engine: "raster") do |i|
  name "rasterizer_engine_lod_#{i - 1}"

  options(lod: i - 1)
  sunlight
  pinhole_camera :position => [0, 0, -3], :zoom => 1.4
  sphere
end
