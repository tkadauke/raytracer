# Doc-render driver for `raytracer::WireframeEngine`.
#
# Hero image: a sphere at default LOD = 0. Picks sphere over box because
# the LOD-invariant box gives no visual cue that a wireframe engine is
# tessellation-driven; the sphere's lat/lon grid immediately tells you
# what's happening.
#
# Property sweep: same sphere across LOD 0..4 so the doubling segment
# count is visible. Sphere at lod=4: 32 lat bands × 64 lon segs = 2048
# quads; the sweep illustrates how the LOD parameter governs render
# fidelity vs. triangle count for every tessellate-having primitive.
#
# All renderings use a tight frame (sphere fills most of the image) so
# the wireframe topology is readable at thumbnail sizes.

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "wireframe_engine"

  pinhole_camera :position => [0, 0, -3], :zoom => 1.4
  sphere
end

property_doc(engine: "wireframe") do |i|
  name "wireframe_engine_lod_#{i - 1}"

  options(lod: i - 1)
  pinhole_camera :position => [0, 0, -3], :zoom => 1.4
  sphere
end
