# Equirectangular cameras need a 2:1 aspect for the projection to
# render with square equator pixels — anything else stretches the
# spheres into ovals (verified empirically when class_doc's default
# 4:3 was used initially). The `aspect: :panoramic` keyword on
# class_doc routes through `render_size(1, aspect: :panoramic)` →
# 640×320.
class_doc aspect: :panoramic do
  name "equirectangular_camera"
  panorama_scene
  equirectangular_camera :position => [0, -1, 0], :target => [0, -1, 1]
end
