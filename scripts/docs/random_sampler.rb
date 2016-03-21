property_doc do |i|
  name "random_sampler_spp_#{i*i}"
  options :sampler => "Random", :samples_per_pixel => i * i

  material_scene red_matte
end
