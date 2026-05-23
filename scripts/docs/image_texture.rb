["nearest", "bilinear", "mipmap"].each do |filter|
  class_doc(:engine => "raster") do
    name "image_texture_filter_#{filter}"
    sunlight
    default_camera(:position => [0.5, -1.6, -4.0], :zoom => 1.6)

    box :position => [0, 1.1, 0],
        :size => [10, 0.1, 10],
        :material => matte_material(
          :diffuseTexture => image_texture(
            :path => "scripts/docs/image_texture_checker.ppm",
            :filter => filter,
            :wrap => "repeat",
            :mapping => "uv",
            :uScale => 256,
            :vScale => 256
          ),
          :ambientCoefficient => 0.35,
          :diffuseCoefficient => 0.65
        )
  end
end
