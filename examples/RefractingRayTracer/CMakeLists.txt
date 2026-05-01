add_executable(RefractingRayTracer
  RefractingRayTracer.cpp
)
target_link_libraries(RefractingRayTracer PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,raytracer>)
