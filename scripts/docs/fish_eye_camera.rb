Scene.new do
  camera_scene
  fish_eye_camera :position => [0, -1, -3]
end.render("docs/images/fish_eye_camera_cube.png")

1.upto(5) do |i|
  fov = 90 + (i - 1) * 60
  
  Scene.new do
    camera_scene
    fish_eye_camera :position => [0, -1, -3], :fieldOfView => fov.degrees
  end.render("docs/images/fish_eye_camera_cube_fov_#{fov}.png", :width => 240, :height => 180)
end
