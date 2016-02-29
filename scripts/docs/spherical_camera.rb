Scene.new do
  camera_scene
  spherical_camera :position => [0, -1, -3]
end.render("docs/images/spherical_camera_cube.png")

1.upto(5) do |i|
  fov = 90 + (i - 1) * 60
  
  Scene.new do
    camera_scene
    spherical_camera :position => [0, -1, -3], :horizontalFieldOfView => fov.degrees
  end.render("docs/images/spherical_camera_cube_horz_fov_#{fov}.png", :width => 240, :height => 180)
end

1.upto(5) do |i|
  fov = 30 + (i - 1) * 30
  
  Scene.new do
    camera_scene
    spherical_camera :position => [0, -1, -3], :verticalFieldOfView => fov.degrees
  end.render("docs/images/spherical_camera_cube_vert_fov_#{fov}.png", :width => 240, :height => 180)
end
