Scene.new do
  object_scene
  box :material => red_matte
end.render("docs/images/box.png")

1.upto(5) do |i|
  offset = (i - 1) * 0.125
  size = [1.0 - offset, 0.5 + offset, 0.5 + offset]
  
  Scene.new do
    object_scene
    box :size => size, :material => red_matte
  end.render("docs/images/box_size_#{i}.png", :width => 240, :height => 180)
end
