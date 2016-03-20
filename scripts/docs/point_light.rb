property_doc do |i|
  name "point_light_position_#{i}"
  
  light_scene
  point_light :position => [(i - 3) * 2, -5, 3], :intensity => 1
end
