1.upto(5) do |i|
  direction = [(i - 3) * 2, -1, -0.5]
  
  Scene.new do
    light_scene
    directional_light :direction => direction, :intensity => 1
  end.render("docs/images/directional_light_direction_#{i}.png", :width => 240, :height => 180)
end
