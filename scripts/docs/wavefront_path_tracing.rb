module ::Common
  def wavefront_path_tracing_scene
    background [0.78, 0.86, 0.95]
    ambient [0.0, 0.0, 0.0]

    pinhole_camera(
      :position => [0.15, -0.35, -4.2],
      :target => [-0.05, 0.2, 0.35],
      :distance => 5.0,
      :zoom => 1.45
    )

    point_light(
      :name => "Key light",
      :position => [-0.55, -1.75, -1.2],
      :color => [1.0, 0.96, 0.88],
      :intensity => 3.2
    )

    red_material = matte_material(
      :diffuseTexture => constant_color_texture(:color => [0.95, 0.08, 0.03]),
      :ambientCoefficient => 0.0,
      :diffuseCoefficient => 1.0
    )
    neutral_material = matte_material(
      :diffuseTexture => constant_color_texture(:color => [0.74, 0.74, 0.70]),
      :ambientCoefficient => 0.0,
      :diffuseCoefficient => 1.0
    )
    white_material = matte_material(
      :diffuseTexture => constant_color_texture(:color => [0.88, 0.86, 0.80]),
      :ambientCoefficient => 0.0,
      :diffuseCoefficient => 1.0
    )

    box(
      :name => "Red bounce wall",
      :position => [-1.9, -0.1, 0.55],
      :size => [0.08, 2.4, 3.8],
      :material => red_material
    )
    box(
      :name => "Diffuse floor",
      :position => [0.0, 1.1, 0.55],
      :size => [3.8, 0.08, 3.8],
      :material => neutral_material
    )
    box(
      :name => "Back wall",
      :position => [0.0, -0.1, 2.45],
      :size => [3.8, 2.4, 0.08],
      :material => white_material
    )
    sphere(
      :name => "Bounce receiver",
      :position => [0.55, 0.52, 0.55],
      :radius => 0.55,
      :material => white_material
    )
  end

end

render_wavefront_path_tracing_doc = lambda do |file, options|
  doc_scene render_size(1, aspect: :default).merge(options) do
    name file
    wavefront_path_tracing_scene
  end
end

render_wavefront_path_tracing_doc.call(
  "wavefront_path_tracing_whitted",
  :direct_engine => true,
  :engine => "raytracer",
  :integrator => "whitted",
  :sampler => "Halton",
  :samples_per_pixel => 16,
  :depth => 4
)

render_wavefront_path_tracing_doc.call(
  "wavefront_path_tracing_scalar_pathtracer",
  :direct_engine => true,
  :engine => "pathtracer",
  :sampler => "Halton",
  :samples_per_pixel => 64,
  :pathtracer_direct_light_samples => 2,
  :depth => 4
)

render_wavefront_path_tracing_doc.call(
  "wavefront_path_tracing_wavefront_pathtracer",
  :direct_engine => true,
  :engine => "wavefront",
  :integrator => "pathtracer",
  :sampler => "Halton",
  :samples_per_pixel => 64,
  :pathtracer_direct_light_samples => 2,
  :depth => 4
)
