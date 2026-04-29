add_executable(GeneratedRayTracer
  GeneratedRayTracer.cpp
  Display.cpp
  Display.h
  MainWindow.cpp
  MainWindow.h
)
set_target_properties(GeneratedRayTracer PROPERTIES AUTOMOC ON)
target_link_libraries(GeneratedRayTracer PRIVATE raytracer)
