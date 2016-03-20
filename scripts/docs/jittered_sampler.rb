property_doc do |i|
  name "jittered_sampler_spp_#{i*i}"
  options :sampler => "Jittered", :samples_per_pixel => i * i

  material_scene red_matte
end
