[true, false].each do |activity|
  class_doc do
    name activity ? "csg_surface" : "csg_surface_inactive"
  
    default_camera
    checker_board
    sunlight
  
    difference do
      active activity
      
      sphere :material => blue_matte
      sphere :radius => 0.8, :material => red_matte
    
      union :material => red_matte do
        cylinder :radius => 0.5, :height => 2.2, :rotation => [90.degrees, 0, 0]
        cylinder :radius => 0.5, :height => 2.2
        cylinder :radius => 0.5, :height => 2.2, :rotation => [0, 0, 90.degrees]
      end
    end
  end
end
