# Glob the library sources. CONFIGURE_DEPENDS asks CMake to re-glob on
# rebuild — matches the existing Rakefile's FileList behaviour, at the cost
# of an extra stat per build. Switch to an explicit list once the file set
# stabilises post-modernization.
file(GLOB_RECURSE RAYTRACER_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# AUTOMOC scans .cpp files for #include "*.moc" and headers reachable via
# target sources. Add the include/ headers to the target so AUTOMOC sees the
# Q_OBJECT classes that live there.
file(GLOB_RECURSE RAYTRACER_HEADERS CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/*.h"
)

add_library(raytracer STATIC
  ${RAYTRACER_SOURCES}
  ${RAYTRACER_HEADERS}
)

target_include_directories(raytracer
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
)

target_link_libraries(raytracer
  PUBLIC
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Script
)

set_target_properties(raytracer PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
  AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/widgets;${CMAKE_CURRENT_SOURCE_DIR}/widgets/world"
)
