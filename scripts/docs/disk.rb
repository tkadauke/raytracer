class_doc(engines: [:raytracer, :raster, :wireframe]) do
  name "disk"

  object_scene
  disk :radius => 1, :material => red_matte
end
