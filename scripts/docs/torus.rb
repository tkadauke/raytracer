class_doc(engines: [:raytracer, :raster, :wireframe]) do
  name "torus"

  object_scene
  torus :sweptRadius => 1.5, :tubeRadius => 0.5, :material => red_matte
end
