require_relative 'lib/scene'
require_relative 'lib/objects'
require_relative 'lib/cameras'
require_relative 'lib/materials'
require_relative 'lib/lights'
require_relative 'lib/matrix'

360.times do |i|
  scene do
    outfile "tmp/sg_#{i.to_s.rjust(3, "0")}.png"
    options :samples_per_pixel => 16
    
    matrix = Matrix.rotateY(i.degrees)
    
    default_camera :position => matrix * [0, -(0.8 + Math.sin(i.degrees)), -3]
    checker_board
    sunlight
  
    difference :material => glass do
      sphere
      sphere :radius => 0.8
      cylinder :radius => 0.5, :height => 2.5
      cylinder :radius => 0.5, :height => 2.5, :rotation => [90.degrees, 0, 0]
      cylinder :radius => 0.5, :height => 2.5, :rotation => [0, 0, 90.degrees]
    end
  end
end
