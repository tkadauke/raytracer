rainbow_doc do |name, color|
  name "light_rainbow_#{name}"
  light_scene
  sunlight :color => color
end

property_doc do |i|
  intensity = (i - 1) * 0.25
  name "light_intensity_#{intensity}"  
  
  light_scene
  sunlight :intensity => intensity
end
