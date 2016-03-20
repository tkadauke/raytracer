class_doc do
  name "checker_board"
  object_scene
end

rainbow_doc do |name, color|
  name "checker_board_bright_color_#{name}"

  sunlight
  checker_board :material => matte_material(
                  :diffuseTexture => checker_board_texture(
                    :brightTexture => constant_color_texture(:color => color),
                    :darkTexture => black,
                  )
                )
  default_camera
end

rainbow_doc do |name, color|
  name "checker_board_dark_color_#{name}"
  
  sunlight
  checker_board :material => matte_material(
                  :diffuseTexture => checker_board_texture(
                    :brightTexture => white,
                    :darkTexture => constant_color_texture(:color => color),
                  )
                )
  default_camera
end
