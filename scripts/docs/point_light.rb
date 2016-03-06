1.upto(5) do |i|
  position = [(i - 3) * 2, -5, 3]
  
  Scene.new do
    light_scene
    point_light :position => position, :intensity => 1
  end.render("docs/images/point_light_position_#{i}.png", :width => 240, :height => 180)
end
