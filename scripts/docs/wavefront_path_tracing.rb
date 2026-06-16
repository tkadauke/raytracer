module ::Common
  def tracing_backend_comparison_scene
    background [0.02, 0.03, 0.04]
    ambient [0.0, 0.0, 0.0]

    pinhole_camera(
      :position => [0.0, 0.75, -4.25],
      :target => [0.0, -0.2, 0.35],
      :distance => 4.0,
      :zoom => 1.35
    )

    point_light(
      :name => "Key light",
      :position => [-2.0, 3.0, -2.0],
      :color => [1.0, 0.96, 0.88],
      :intensity => 1.15
    )

    sphere_material = matte_material(
      :diffuseTexture => constant_color_texture(:color => [0.8, 0.18, 0.12]),
      :ambientCoefficient => 0.0,
      :diffuseCoefficient => 1.0
    )
    floor_material = matte_material(
      :diffuseTexture => constant_color_texture(:color => [0.68, 0.7, 0.72]),
      :ambientCoefficient => 0.0,
      :diffuseCoefficient => 1.0
    )

    sphere(
      :name => "Matte sphere",
      :position => [-0.35, -0.05, 0.35],
      :radius => 0.7,
      :material => sphere_material
    )
    rectangle(
      :name => "Matte floor",
      :position => [-2.5, -0.8, -1.5],
      :leg1 => [0.0, 0.0, 4.0],
      :leg2 => [5.0, 0.0, 0.0],
      :material => floor_material
    )
  end

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

render_tracing_backend_comparison_doc = lambda do |file, backend|
  doc_scene render_size(1, aspect: :default).merge(
    :direct_engine => true,
    :engine => "wavefront",
    :integrator => "pathtracer",
    :wavefront_intersection_backend => backend,
    :wavefront_metrics_out => "docs/images/#{file}_metrics.json",
    :wavefront_metrics_summary => true,
    :wavefront_denoiser => "none",
    :sampler => "Regular",
    :samples_per_pixel => 1,
    :sampling_seed => 1337,
    :pathtracer_direct_light_samples => 1,
    :depth => 1
  ) do
    name file
    tracing_backend_comparison_scene
  end
end

render_tracing_backend_comparison_doc.call(
  "tracing_backend_comparison_cpu",
  "cpu"
)

render_tracing_backend_comparison_doc.call(
  "tracing_backend_comparison_auto",
  "auto"
)

render_tracing_backend_comparison_doc.call(
  "tracing_backend_comparison_gpu_request",
  "gpu"
)
