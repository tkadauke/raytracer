property_doc do |i|
  name "regular_sampler_spp_#{i*i}"
  options :sampler => "Regular", :samples_per_pixel => i * i
  
  material_scene red_matte
end
