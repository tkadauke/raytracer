class_doc do
  name "dice"
  
  object_scene
  scripted_surface :scriptName => 'scripts/dice.js', :diceMaterial => red_matte, :dotMaterial => white_matte
end

class_doc do
  name "brick"
  
  object_scene
  scripted_surface :scriptName => 'scripts/brick.js', :color => red_matte
end