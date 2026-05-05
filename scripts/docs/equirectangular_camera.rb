# Equirectangular cameras need a 2:1 aspect output for the projection
# to render with square equator pixels — anything else stretches the
# spheres into ovals (verified empirically when class_doc's default
# 640×480 was used initially). doc_scene exposes width/height options;
# class_doc / property_doc wrap it with fixed sizes that don't fit.
doc_scene :width => 640, :height => 320 do
  name "equirectangular_camera"
  panorama_scene
  equirectangular_camera :position => [0, -1, 0], :target => [0, -1, 1]
end
