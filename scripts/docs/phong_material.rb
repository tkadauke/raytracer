Scene.new do
  material_scene phong_material(:diffuseTexture => red)
end.render("docs/images/phong_material_red.png")

rainbow_colors.each do |name, color|
  Scene.new do
    material_scene phong_material(:diffuseTexture => white, :specularColor => color)
  end.render("docs/images/phong_material_specular_color_#{name}.png", :width => 160, :height => 120)
end

1.upto(5) do |i|
  coeff = (i - 1) * 0.25

  Scene.new do
    material_scene phong_material(:diffuseTexture => red, :specularCoefficient => coeff)
  end.render("docs/images/phong_material_specular_coeff_#{coeff}.png", :width => 240, :height => 180)
end

1.upto(5) do |i|
  exp = i ** 3

  Scene.new do
    material_scene phong_material(:diffuseTexture => red, :exponent => exp)
  end.render("docs/images/phong_material_exponent_#{exp}.png", :width => 240, :height => 180)
end
