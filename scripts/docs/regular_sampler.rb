1.upto(5) do |i|
  Scene.new do
    material_scene red_matte
  end.render("docs/images/regular_sampler_spp_#{i*i}.png", :sampler => "Regular", :samples_per_pixel => i * i, :width => 240, :height => 180)
end
