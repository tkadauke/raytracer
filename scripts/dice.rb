require_relative 'lib/scene'
require_relative 'lib/objects'
require_relative 'lib/cameras'
require_relative 'lib/materials'
require_relative 'lib/lights'

module Dice
  def dice(attrs = {})
    difference attrs do
      box :bevelRadius => 0.15, :material => red_matte
      dots 1, :rotation => [180.degrees, 0, 0]
      dots 2, :rotation => [90.degrees, 0, 0]
      dots 3, :rotation => [0, 90.degrees, 0]
      dots 4, :rotation => [0, 270.degrees, 0]
      dots 5, :rotation => [270.degrees, 0, 0]
      dots 6, :rotation => [0, 0, 0]
    end
  end
  
  def dots(num, attrs = {})
    default_attrs = {
      :material => white_matte
    }
    union default_attrs.merge(attrs) do
      sphere :radius => 0.3, :position => [-0.5, -0.5, -1.22] if [4, 5, 6].include?(num)
      sphere :radius => 0.3, :position => [-0.5,  0.0, -1.22] if [6].include?(num)
      sphere :radius => 0.3, :position => [-0.5,  0.5, -1.22] if [2, 3, 4, 5, 6].include?(num)
      sphere :radius => 0.3, :position => [ 0.0,  0.0, -1.22] if [1, 3, 5].include?(num)
      sphere :radius => 0.3, :position => [ 0.5, -0.5, -1.22] if [2, 3, 4, 5, 6].include?(num)
      sphere :radius => 0.3, :position => [ 0.5,  0.0, -1.22] if [6].include?(num)
      sphere :radius => 0.3, :position => [ 0.5,  0.5, -1.22] if [4, 5, 6].include?(num)
    end
  end
end

ElementCreator.send :include, Dice

scene do
  outfile "tmp/dice.png"
  options :samples_per_pixel => 1
  
  default_camera
  checker_board
  sunlight
  
  dice
end
