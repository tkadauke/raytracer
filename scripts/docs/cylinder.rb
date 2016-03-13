Scene.new do
  object_scene
  cylinder :material => red_matte
end.render("docs/images/cylinder.png")

1.upto(5) do |i|
  radius = 0.5 + (i - 1) * 0.125
  
  Scene.new do
    object_scene
    cylinder :radius => radius, :material => red_matte
  end.render("docs/images/cylinder_radius_#{i}.png", :width => 240, :height => 180)
end

1.upto(5) do |i|
  height = 1 + (i - 1) * 0.25
  
  Scene.new do
    object_scene
    cylinder :height => height, :material => red_matte
  end.render("docs/images/cylinder_height_#{i}.png", :width => 240, :height => 180)
end
