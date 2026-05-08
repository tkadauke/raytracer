class_doc(engines: [:raytracer, :raster, :wireframe]) do
  name "triangle"

  sunlight
  checker_board
  pinhole_camera :position => [0, -0.33, -2.0],
                 :target => [0, -0.33, 0.15],
                 :zoom => 2.0
  triangle :vertexA => [1, 0, 0],
           :vertexB => [0, -1, 0.3],
           :vertexC => [-1, 0, 0],
           :material => red_matte
end
