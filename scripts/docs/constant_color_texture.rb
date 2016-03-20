rainbow_doc do |name, color|
  name "constant_color_#{name}"

  sunlight
  box :material => matte_material(
        :diffuseTexture => constant_color_texture(:color => color)
      )
  default_camera
end
