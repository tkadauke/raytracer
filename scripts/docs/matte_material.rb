Scene.new do
  material_scene red_matte
end.render("docs/images/matte_material_red.png")

rainbow_colors.each do |name, color|
  Scene.new do
    material_scene matte_material(:diffuseTexture => constant_color_texture(:color => color))
  end.render("docs/images/matte_material_rainbow_#{name}.png", :width => 160, :height => 120)
end

1.upto(5) do |i|
  coeff = (i - 1) * 0.25
  
  Scene.new do
    material_scene matte_material(:diffuseTexture => green, :ambientCoefficient => coeff)
  end.render("docs/images/matte_material_ambient_#{coeff}.png", :width => 240, :height => 180)
end

1.upto(5) do |i|
  coeff = (i - 1) * 0.5
  
  Scene.new do
    material_scene matte_material(:diffuseTexture => green, :diffuseCoefficient => coeff)
  end.render("docs/images/matte_material_diffuse_#{coeff}.png", :width => 240, :height => 180)
end
