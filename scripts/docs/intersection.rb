class_doc do
  name "intersection"
  
  object_scene
  intersection do
    box :material => red_matte
    sphere :position => [-1, -1, -1], :material => blue_matte
  end
end
