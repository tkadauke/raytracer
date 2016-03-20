class_doc do
  name "difference"
  
  object_scene
  difference do
    box :material => red_matte
    sphere :position => [-1, -1, -1], :material => blue_matte
  end
end
