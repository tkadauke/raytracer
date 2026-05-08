# Wireframe-engine renderings, one per primitive that produces a
# non-empty mesh from `tessellate()`. Each is referenced from the
# matching primitive's docstring as `@image html <name>_wireframe.png`.
#
# Skipped on purpose:
#   - Plane: tessellates to empty mesh (infinite — can't be finitely
#     covered).
#   - CSG ops (Difference / Union / Intersection / MinkowskiSum /
#     ConvexHull): empty meshes pending §4.2.a mesh booleans.
#   - Composite / Instance / Grid / Scene: aggregators with no
#     intrinsic geometry — their wireframe is just their children's.
#   - FlatMeshTriangle / SmoothMeshTriangle: internal to a Mesh
#     primitive (PLY-loaded). Pending the world::Mesh wrapper
#     (separate task).
#
# All renderings use a 320×240 frame with a camera positioned to make
# the primitive fill the frame — wireframes convey the same
# information as full-shaded renders at smaller sizes, and a tightly-
# framed subject reads better as a docs thumbnail than a small subject
# in a sea of black.

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "box_wireframe"
  pinhole_camera :position => [1.5, -1.0, -2.5], :zoom => 1.4
  box
end

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "sphere_wireframe"
  pinhole_camera :position => [0, 0, -3], :zoom => 1.4
  sphere
end

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "torus_wireframe"
  pinhole_camera :position => [0, -2, -3.5], :zoom => 1.4
  torus :sweptRadius => 1.5, :tubeRadius => 0.5
end

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "cylinder_wireframe"
  pinhole_camera :position => [1.2, -1.2, -3], :zoom => 1.4
  cylinder :radius => 1, :height => 2
end

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "open_cylinder_wireframe"
  pinhole_camera :position => [1.2, -1.2, -3], :zoom => 1.4
  open_cylinder :radius => 1, :height => 2
end

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "disk_wireframe"
  pinhole_camera :position => [0, -0.9, -1.4], :zoom => 3.0
  disk :radius => 1
end

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "triangle_wireframe"
  # Aim at the triangle's centroid, not the world origin — the
  # default vertices straddle y=0 so the centroid is offset down.
  pinhole_camera :position => [0, -0.33, -1.0], :target => [0, -0.33, 0], :zoom => 2.5
  triangle
end

class_doc(engine: "wireframe", width: 320, height: 240) do
  name "rectangle_wireframe"
  # Aim at the rectangle's center (0.5, 0, 0.5) since the default
  # legs anchor the corner at the origin.
  pinhole_camera :position => [0.5, -0.7, -0.7], :target => [0.5, 0, 0.5], :zoom => 3.5
  rectangle
end
