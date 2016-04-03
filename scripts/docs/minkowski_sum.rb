class_doc do
  name "minkowski_sum"
  
  object_scene
  minkowski_sum :material => red_matte do
    box :size => [1, 0.2, 1]
    cylinder :height => 0.2
  end
end
