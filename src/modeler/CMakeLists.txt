add_executable(Modeler
  modeler.cpp
  Display.cpp
  Display.h
  MainWindow.cpp
  MainWindow.h
)
set_target_properties(Modeler PROPERTIES AUTOMOC ON)
target_link_libraries(Modeler PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,raytracer>)
