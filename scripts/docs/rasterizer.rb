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

class_doc(engine: "raster", width: 320, height: 240) do
  name "rasterizer_uv_albedo"

  ambient [1, 1, 1]
  background [0.03, 0.03, 0.035]
  pinhole_camera :position => [0, 0, -4], :target => [0, 0, 0], :zoom => 1.25
  rectangle :position => [-1.3, 0.9, 0],
            :leg1 => [2.6, 0, 0],
            :leg2 => [0, -1.8, 0],
            :material => matte_material(:diffuseTexture => uv_color_texture)
end

class_doc(engine: "raster", width: 320, height: 240) do
  name "rasterizer_uv_checker"

  options(lod: 3)
  ambient [1, 1, 1]
  background [0.03, 0.03, 0.035]
  pinhole_camera :position => [0.35, 0.15, -4.2], :target => [0, 0, 0], :zoom => 1.2
  box :size => [1.5, 1.2, 1.0],
      :rotation => [0.25, -0.55, 0],
      :material => matte_material(
        :diffuseTexture => checker_board_texture(
          :mapping => "uv",
          :uScale => 8,
          :vScale => 8,
          :brightTexture => white,
          :darkTexture => black,
        )
      )
end
