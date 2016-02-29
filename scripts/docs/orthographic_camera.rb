Scene.new do
  camera_scene
  orthographic_camera :position => [0, -1, -3]
end.render("docs/images/orthographic_camera_cube.png")

1.upto(5) do |i|
  zoom = 1 + (i - 1) * 0.25
  
  Scene.new do
    camera_scene
    orthographic_camera :position => [0, -1, -3], :zoom => zoom
  end.render("docs/images/orthographic_camera_cube_zoom_#{zoom}.png", :width => 240, :height => 180)
end
