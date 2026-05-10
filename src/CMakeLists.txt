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

file(GLOB_RECURSE RAYTRACER_UIS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
)

file(GLOB_RECURSE RAYTRACER_PRIVATE_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(raytracer STATIC
  ${RAYTRACER_SOURCES}
  ${RAYTRACER_HEADERS}
  ${RAYTRACER_UIS}
  ${RAYTRACER_PRIVATE_HEADERS}
)

target_include_directories(raytracer
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
)

target_link_libraries(raytracer
  PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
)

if(RAYTRACER_ENABLE_STATS)
  # PUBLIC so headers like include/render/Stats.h see the same definition
  # in the library and in dependents (tests, examples, tools). Without that,
  # a translation unit could include Stats.h with the macro undefined and
  # silently emit no-op increments while the library counted them.
  target_compile_definitions(raytracer PUBLIC RAYTRACER_ENABLE_STATS)
endif()

set_target_properties(raytracer PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
  AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/widgets;${CMAKE_CURRENT_SOURCE_DIR}/widgets/world"
)
