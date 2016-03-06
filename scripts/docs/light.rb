rainbow_colors.each do |name, color|
  Scene.new do
    light_scene
    sunlight :color => color
  end.render("docs/images/light_rainbow_#{name}.png", :width => 160, :height => 120)
end

1.upto(5) do |i|
  intensity = (i - 1) * 0.25
  
  Scene.new do
    light_scene
    sunlight :intensity => intensity
  end.render("docs/images/light_intensity_#{intensity}.png", :width => 240, :height => 180)
end
