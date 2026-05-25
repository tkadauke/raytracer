# Render graph docs driver.
#
# These images show a concrete graph-backed render next to a graph-produced AOV
# from the same scene. The scene avoids a ground plane so the stencil AOV reads
# as object coverage instead of a mostly-filled floor mask.

module ::Common
  def render_graph_aov_scene
    options(lod: 4)
    ambient [0.34, 0.34, 0.36]
    background [0.10, 0.13, 0.16]

    point_light :position => [0.0, -2.8, -4.0],
                :color => [1.0, 0.96, 0.86],
                :intensity => 2.0

    pinhole_camera :position => [0.0, 0.0, -5.2],
                   :target => [0.0, 0.0, 0.05],
                   :zoom => 1.95

    torus :sweptRadius => 0.50,
          :tubeRadius => 0.15,
          :position => [-1.32, 0.00, 0.10],
          :rotation => [0.42, -0.38, 0.0],
          :material => phong_material(
            :diffuseTexture => constant_color_texture(:color => [0.88, 0.18, 0.12]),
            :ambientCoefficient => 0.36,
            :diffuseCoefficient => 0.72,
            :specularCoefficient => 0.75,
            :exponent => 32,
          )

    sphere :radius => 0.50,
           :position => [0.0, 0.02, 0.00],
           :material => matte_material(
             :diffuseTexture => constant_color_texture(:color => [0.12, 0.62, 0.32]),
             :ambientCoefficient => 0.34,
             :diffuseCoefficient => 0.95,
           )

    box :size => [0.66, 0.82, 0.66],
        :position => [1.25, 0.03, 0.08],
        :rotation => [0.15, 0.48, -0.08],
        :material => phong_material(
          :diffuseTexture => constant_color_texture(:color => [0.18, 0.36, 0.86]),
          :ambientCoefficient => 0.36,
          :diffuseCoefficient => 0.72,
          :specularCoefficient => 0.45,
          :exponent => 18,
        )
  end
end

GRAPH_DOC_OPTIONS = {
  :render_graph => true,
  :engine => "raster",
  :width => 480,
  :height => 270,
  :samples_per_pixel => 1,
}

class_doc(**GRAPH_DOC_OPTIONS) do
  name "render_graph_raster_beauty"
  render_graph_aov_scene
end

class_doc(**GRAPH_DOC_OPTIONS, :render_graph_view => "stencil") do
  name "render_graph_raster_stencil_aov"
  render_graph_aov_scene
end
