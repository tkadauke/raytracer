# Doc-render driver for `engine::raster::Rasterizer`.
#
# Hero image: a sphere at default LOD = 0. Same scene-shape as
# `wireframe_engine.rb` so the wireframe-vs-rasterizer comparison
# is direct: same primitive, same camera, just a filled rendering
# instead of edges.
#
# Property sweep: same sphere across LOD 0..4. Higher LOD =
# denser triangulation = smoother shaded silhouette (the
# per-face hash colors blur out as triangles shrink past the
# pixel size).

module ::Common
  def rasterizer_material_preview_scene
    options(lod: 4)
    ambient [0.10, 0.10, 0.11]
    background [0.035, 0.04, 0.05]

    point_light :position => [0.0, -3.1, -5.0],
                :color => [1.0, 0.96, 0.88],
                :intensity => 1.55

    pinhole_camera :position => [0.0, -2.25, -7.15],
                   :target => [0.0, -0.02, 0.05],
                   :zoom => 1.28

    rectangle :position => [-3.8, 1.02, -1.85],
              :leg1 => [7.6, 0, 0],
              :leg2 => [0, 0, 4.4],
              :material => matte_material(
                :diffuseTexture => checker_board_texture(
                  :uScale => 1.25,
                  :vScale => 1.25,
                  :brightTexture => constant_color_texture(:color => [0.44, 0.44, 0.42]),
                  :darkTexture => constant_color_texture(:color => [0.20, 0.22, 0.24]),
                )
              )

    sphere :radius => 0.52,
           :position => [-2.25, 0.20, 0.00],
           :material => matte_material(
             :diffuseTexture => constant_color_texture(:color => [0.88, 0.13, 0.08]),
             :ambientCoefficient => 0.12,
             :diffuseCoefficient => 1.0,
           )

    sphere :radius => 0.52,
           :position => [-0.75, 0.20, 0.00],
           :material => matte_material(
             :diffuseTexture => constant_color_texture(:color => [0.10, 0.72, 0.24]),
             :ambientCoefficient => 1.25,
             :diffuseCoefficient => 0.55,
           )

    sphere :radius => 0.52,
           :position => [0.75, 0.20, 0.00],
           :material => phong_material(
             :diffuseTexture => constant_color_texture(:color => [0.13, 0.32, 0.88]),
             :ambientCoefficient => 0.20,
             :diffuseCoefficient => 0.70,
             :specularColor => [1.0, 1.0, 1.0],
             :specularCoefficient => 1.0,
             :exponent => 8,
           )

    sphere :radius => 0.52,
           :position => [2.25, 0.20, 0.00],
           :material => phong_material(
             :diffuseTexture => constant_color_texture(:color => [0.82, 0.58, 0.10]),
             :ambientCoefficient => 0.20,
             :diffuseCoefficient => 0.70,
             :specularColor => [1.0, 1.0, 1.0],
             :specularCoefficient => 1.0,
             :exponent => 90,
           )
  end

  def rasterizer_shadow_scene(light_direction: [-0.52, -0.78, -0.28])
    options(lod: 3)
    ambient [0.18, 0.18, 0.20]
    background [0.13, 0.16, 0.20]

    directional_light :direction => light_direction,
                      :color => [1.0, 0.96, 0.86],
                      :intensity => 1.35

    pinhole_camera :position => [2.45, -1.70, -3.65],
                   :target => [0.12, 0.52, -0.02],
                   :zoom => 2.15

    rectangle :position => [-3.1, 1.05, -2.2],
              :leg1 => [6.2, 0, 0],
              :leg2 => [0, 0, 4.4],
              :material => matte_material(
                :diffuseTexture => checker_board_texture(
                  :uScale => 1.25,
                  :vScale => 1.25,
                  :brightTexture => constant_color_texture(:color => [0.78, 0.76, 0.64]),
                  :darkTexture => constant_color_texture(:color => [0.38, 0.43, 0.47]),
                )
              )

    sphere :radius => 0.82,
           :position => [-0.95, 0.24, -0.15],
           :material => matte_material(:diffuseTexture => constant_color_texture(
             :color => [0.76, 0.16, 0.12]
           ))

    box :size => [0.82, 1.02, 0.88],
        :position => [0.62, 0.55, 0.05],
        :rotation => [0.0, 0.32, 0.0],
        :material => matte_material(:diffuseTexture => constant_color_texture(
          :color => [0.13, 0.35, 0.74]
        ))

    cylinder :radius => 0.27,
             :height => 0.72,
             :position => [1.28, 0.70, -0.72],
             :material => matte_material(:diffuseTexture => constant_color_texture(
               :color => [0.77, 0.66, 0.10]
             ))
  end

  def rasterizer_shadow_cascade_scene
    options(lod: 3)
    ambient [0.18, 0.18, 0.20]
    background [0.13, 0.16, 0.20]

    directional_light :direction => [-0.52, -0.78, -0.28],
                      :color => [1.0, 0.96, 0.86],
                      :intensity => 1.35

    pinhole_camera :position => [2.55, -1.92, -4.85],
                   :target => [0.05, 0.58, 1.65],
                   :zoom => 1.55

    rectangle :position => [-3.35, 1.05, -2.4],
              :leg1 => [6.7, 0, 0],
              :leg2 => [0, 0, 10.4],
              :material => matte_material(
                :diffuseTexture => checker_board_texture(
                  :uScale => 1.0,
                  :vScale => 3.0,
                  :brightTexture => constant_color_texture(:color => [0.78, 0.76, 0.64]),
                  :darkTexture => constant_color_texture(:color => [0.38, 0.43, 0.47]),
                )
              )

    sphere :radius => 0.72,
           :position => [-0.92, 0.28, -0.22],
           :material => matte_material(:diffuseTexture => constant_color_texture(
             :color => [0.76, 0.16, 0.12]
           ))

    box :size => [0.74, 0.96, 0.82],
        :position => [0.62, 0.57, 0.12],
        :rotation => [0.0, 0.32, 0.0],
        :material => matte_material(:diffuseTexture => constant_color_texture(
          :color => [0.13, 0.35, 0.74]
        ))

    box :size => [0.65, 1.00, 0.65],
        :position => [1.15, 0.55, 6.15],
        :rotation => [0.0, -0.45, 0.0],
        :material => matte_material(:diffuseTexture => constant_color_texture(
          :color => [0.77, 0.66, 0.10]
        ))
  end
end

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

class_doc(engine: "raster", width: 480, height: 240) do
  name "rasterizer_material_preview"

  rasterizer_material_preview_scene
end

class_doc(engine: "raster", width: 320, height: 180, msaa: 1) do
  name "rasterizer_msaa_1x"

  ambient [1, 1, 1]
  background [0, 0, 0]
  pinhole_camera :position => [0, 0, -4], :target => [0, 0, 0], :zoom => 1.45
  triangle :vertexA => [-1.7, -1.05, 0],
           :vertexB => [ 1.7, -1.05, 0],
           :vertexC => [-1.7,  1.05, 0],
           :material => matte_material(:diffuseTexture => white)
end

class_doc(engine: "raster", width: 320, height: 180, msaa: 1, post_aa: "fxaa") do
  name "rasterizer_post_aa_fxaa"

  ambient [1, 1, 1]
  background [0, 0, 0]
  pinhole_camera :position => [0, 0, -4], :target => [0, 0, 0], :zoom => 1.45
  triangle :vertexA => [-1.7, -1.05, 0],
           :vertexB => [ 1.7, -1.05, 0],
           :vertexC => [-1.7,  1.05, 0],
           :material => matte_material(:diffuseTexture => white)
end

class_doc(engine: "raster", width: 320, height: 240) do
  name "rasterizer_shadow_maps_off"

  rasterizer_shadow_scene
end

class_doc(engine: "raster", width: 320, height: 240,
          shadow_maps: true, shadow_map_size: 256, shadow_bias: 0.18) do
  name "rasterizer_shadow_maps_on"

  rasterizer_shadow_scene
end

shadow_map_sizes = [32, 64, 128, 256, 512]
property_doc(5, engine: "raster", shadow_maps: true) do |i|
  size = shadow_map_sizes[i - 1]
  name "rasterizer_shadow_map_size_#{size}"

  options(shadow_map_size: size, shadow_bias: 0.18)
  rasterizer_shadow_scene
end

shadow_cascade_counts = [1, 2, 4]
shadow_cascade_counts.each do |count|
  class_doc(engine: "raster", width: 320, height: 240,
            shadow_maps: true, shadow_map_size: 64, shadow_bias: 0.35,
            shadow_cascades: count, shadow_cascade_split: 0.5) do
    name "rasterizer_shadow_cascades_#{count}"

    rasterizer_shadow_cascade_scene
  end
end

shadow_cascade_splits = [
  ["0_00", 0.0],
  ["0_50", 0.5],
  ["1_00", 1.0],
]
shadow_cascade_splits.each do |label, split|
  class_doc(engine: "raster", width: 320, height: 240,
            shadow_maps: true, shadow_map_size: 64, shadow_bias: 0.35,
            shadow_cascades: 4, shadow_cascade_split: split) do
    name "rasterizer_shadow_cascade_split_#{label}"

    rasterizer_shadow_cascade_scene
  end
end

shadow_biases = [
  ["0_030", 0.030],
  ["0_060", 0.060],
  ["0_080", 0.080],
  ["0_250", 0.250],
  ["1_500", 1.500],
]
property_doc(5, engine: "raster", shadow_maps: true, shadow_map_size: 256) do |i|
  label, bias = shadow_biases[i - 1]
  name "rasterizer_shadow_bias_#{label}"

  options(shadow_bias: bias)
  rasterizer_shadow_scene
end

shadow_slope_biases = [
  ["0_000", 0.000],
  ["0_005", 0.005],
  ["0_020", 0.020],
  ["0_050", 0.050],
  ["0_200", 0.200],
]
property_doc(5, engine: "raster", shadow_maps: true, shadow_map_size: 256,
                shadow_bias: 0.03) do |i|
  label, bias = shadow_slope_biases[i - 1]
  name "rasterizer_shadow_slope_bias_#{label}"

  options(shadow_slope_bias: bias)
  rasterizer_shadow_scene
end

shadow_filter_radii = [0, 1, 2, 3, 4]
property_doc(5, engine: "raster", shadow_maps: true, shadow_map_size: 128, shadow_bias: 0.25) do |i|
  radius = shadow_filter_radii[i - 1]
  name "rasterizer_shadow_filter_radius_#{radius}"

  options(shadow_filter_radius: radius)
  rasterizer_shadow_scene
end

class_doc(engine: "raster", width: 320, height: 240, shadow_maps: true,
          shadow_map_size: 128, shadow_bias: 0.25, shadow_filter_radius: 4,
          shadow_filter: "pcf") do
  name "rasterizer_shadow_filter_mode_pcf"

  rasterizer_shadow_scene
end

class_doc(engine: "raster", width: 320, height: 240, shadow_maps: true,
          shadow_map_size: 128, shadow_bias: 0.25, shadow_filter_radius: 4,
          shadow_filter: "pcss") do
  name "rasterizer_shadow_filter_mode_pcss"

  rasterizer_shadow_scene
end

class_doc(engine: "raster", width: 320, height: 180, msaa: 4) do
  name "rasterizer_msaa_4x"

  ambient [1, 1, 1]
  background [0, 0, 0]
  pinhole_camera :position => [0, 0, -4], :target => [0, 0, 0], :zoom => 1.45
  triangle :vertexA => [-1.7, -1.05, 0],
           :vertexB => [ 1.7, -1.05, 0],
           :vertexC => [-1.7,  1.05, 0],
           :material => matte_material(:diffuseTexture => white)
end
