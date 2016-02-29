Scene.new do
  material_scene transparent_material(:diffuseTexture => medium_grey, :refractionIndex => 1.01, :ambientCoefficient => 0.1, :diffuseCoefficient => 0)
end.render("docs/images/transparent_material.png")

1.upto(7) do |i|
  ior = 1.01 + 0.02 * (i - 1)
  
  Scene.new do
    material_scene transparent_material(:diffuseTexture => medium_grey, :refractionIndex => ior, :ambientCoefficient => 0.1, :diffuseCoefficient => 0)
  end.render("docs/images/transparent_material_ior_#{ior}.png", :width => 160, :height => 120)
end
