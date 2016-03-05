rainbow_colors.each do |name, color|
  Scene.new(:ambient => color) do
    object_scene
    sphere :material => reflective_material(:diffuseTexture => nil, :reflectionColor => [1, 1, 1], :reflectionCoefficient => 0.5)
  end.render("docs/images/scene_ambient_#{name}.png", :width => 160, :height => 120)
end

rainbow_colors.each do |name, color|
  Scene.new(:background => color) do
    object_scene
    sphere :material => reflective_material(:diffuseTexture => nil, :reflectionColor => [1, 1, 1], :reflectionCoefficient => 0.5)
  end.render("docs/images/scene_background_#{name}.png", :width => 160, :height => 120)
end
