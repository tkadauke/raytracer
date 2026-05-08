# Tonemap comparison driver — renders the same scene through each
# of the three operators registered with TonemapFactory so the
# Tonemap.h docstring's claims ("Linear is a hard clamp," "Reinhard
# compresses highlights," "ACES has punchy midtones") have rendered
# evidence to point at.
#
# The dof_scene helper gives the operators something to chew on:
# - A bright sky-blue background (1.0 saturation in green/blue).
# - White ambient hitting the matte spheres → near-saturation
#   in their channels.
# - A reflective checker floor with white cells → bright surfaces
#   that the rolloff curves can compress.
#
# The differences are still moderate (the scene is mostly LDR — no
# emissive surfaces, no HDR environment map yet), but the relative
# behavior reads clearly side-by-side: Reinhard darker overall,
# ACES bluer in the sky and punchier in the spheres' midtones.

# All three renders use the same sampler + spp so the only thing
# changing between them is the tonemap.
TONEMAP_RENDER_OPTIONS = { :sampler => "Regular", :samples_per_pixel => 1 }

%w[Linear Reinhard ACES].each do |tonemap|
  filename = "tonemap_#{tonemap.downcase}"
  class_doc(**TONEMAP_RENDER_OPTIONS, :tonemap => tonemap) do
    name filename
    dof_scene
    pinhole_camera :position => [0.5, -1, -3], :zoom => 2
  end
end
