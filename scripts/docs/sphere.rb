Scene.new do
  object_scene
  sphere :material => red_matte
end.render("docs/images/sphere.png")

1.upto(5) do |i|
  size = 0.5 + (i - 1) * 0.125
  
  Scene.new do
    object_scene
    sphere :radius => size, :material => red_matte
  end.render("docs/images/sphere_size_#{i}.png", :width => 240, :height => 180)
end
