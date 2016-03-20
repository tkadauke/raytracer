Scene.new do
  object_scene
end.render("docs/images/checker_board.png")

rainbow_colors.each do |name, color|
  Scene.new do
    sunlight
    checker_board :material => matte_material(
                    :diffuseTexture => checker_board_texture(
                      :brightTexture => constant_color_texture(:color => color),
                      :darkTexture => black,
                    )
                  )
    default_camera
  end.render("docs/images/checker_board_bright_color_#{name}.png", :width => 160, :height => 120)
end

rainbow_colors.each do |name, color|
  Scene.new do
    sunlight
    checker_board :material => matte_material(
                    :diffuseTexture => checker_board_texture(
                      :brightTexture => white,
                      :darkTexture => constant_color_texture(:color => color),
                    )
                  )
    default_camera
  end.render("docs/images/checker_board_dark_color_#{name}.png", :width => 160, :height => 120)
end
