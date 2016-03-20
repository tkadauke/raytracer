class_doc do
  name "sphere"
  object_scene
  sphere :material => red_matte
end

property_doc do |i|
  name "sphere_size_#{i}"
  
  object_scene
  sphere :radius => 0.5 + (i - 1) * 0.125, :material => red_matte
end
