property_doc do |i|
  name "directional_light_direction_#{i}"
  
  light_scene
  directional_light :direction => [(i - 3) * 2, -1, -0.5], :intensity => 1
end
