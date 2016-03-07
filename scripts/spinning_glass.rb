require_relative 'lib/scene'
require_relative 'lib/objects'
require_relative 'lib/cameras'
require_relative 'lib/materials'
require_relative 'lib/lights'

90.times do |i|
  Scene.new do
    default_camera
    checker_board
    sunlight
    
    difference :material => glass, :rotation => [ 0, i.degrees, 0 ] do
      box
      box :size => [ 1.1, 0.5, 0.5 ]
      box :size => [ 0.5, 1.1, 0.5 ]
      box :size => [ 0.5, 0.5, 1.1 ]
    end
  end.render("tmp/frame_#{i.to_s.rjust(3, "0")}.png", :samples_per_pixel => 36)
end
