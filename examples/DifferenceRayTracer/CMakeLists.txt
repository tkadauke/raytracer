add_executable(DifferenceRayTracer
  DifferenceRayTracer.cpp
)
target_link_libraries(DifferenceRayTracer PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,raytracer>)
