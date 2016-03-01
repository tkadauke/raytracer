Scene.new do
  material_scene reflective_material(:diffuseTexture => nil, :reflectionColor => [1, 1, 1], :reflectionCoefficient => 0.5)
end.render("docs/images/reflective_material_red.png")

rainbow_colors.each do |name, color|
  Scene.new do
    material_scene reflective_material(:diffuseTexture => nil, :reflectionColor => color, :reflectionCoefficient => 0.5)
  end.render("docs/images/reflective_material_reflection_color_#{name}.png", :width => 160, :height => 120)
end

1.upto(5) do |i|
  coeff = (i - 1) * 0.25

  Scene.new do
    material_scene reflective_material(:diffuseTexture => nil, :reflectionColor => [1, 1, 1], :reflectionCoefficient => coeff)
  end.render("docs/images/reflective_material_reflection_coeff_#{coeff}.png", :width => 240, :height => 180)
end
