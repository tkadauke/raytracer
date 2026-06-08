# Glob the library sources. CONFIGURE_DEPENDS asks CMake to re-glob on
# rebuild — matches the existing Rakefile's FileList behaviour, at the cost
# of an extra stat per build. Switch to an explicit list once the file set
# stabilises post-modernization.
file(GLOB_RECURSE RAYTRACER_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER RAYTRACER_SOURCES EXCLUDE REGEX "/modeler/")

# AUTOMOC scans .cpp files for #include "*.moc" and headers reachable via
# target sources. Add the include/ headers to the target so AUTOMOC sees the
# Q_OBJECT classes that live there.
file(GLOB_RECURSE RAYTRACER_HEADERS CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE RAYTRACER_UIS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
)
list(FILTER RAYTRACER_UIS EXCLUDE REGEX "/modeler/")

add_library(raytracer STATIC
  ${RAYTRACER_SOURCES}
  ${RAYTRACER_HEADERS}
  ${RAYTRACER_UIS}
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::OpenGL
    ZLIB::ZLIB
)

if(APPLE)
  # CGLContext uses OpenGL.framework directly so that rendercli can
  # open an offscreen GL context without Qt's QGuiApplication.
  target_link_libraries(raytracer PUBLIC "-framework OpenGL")
endif()

if(RAYTRACER_ENABLE_METAL_WAVEFRONT)
  target_sources(raytracer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalWavefrontSmokeKernel.mm")
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalWavefrontSmokeKernel.mm"
    PROPERTIES COMPILE_FLAGS "-fobjc-arc"
  )
  target_compile_definitions(raytracer PUBLIC RAYTRACER_ENABLE_METAL_WAVEFRONT)
  target_link_libraries(raytracer PUBLIC "-framework Metal" "-framework Foundation")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # EGLContext uses Mesa's surfaceless EGL platform so that rendercli
  # can open an offscreen GL context on Linux without QGuiApplication
  # or any window-system surface. libEGL and libGL ship with the
  # `libgl1-mesa-dev` package the Dockerfile installs.
  find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
  target_link_libraries(raytracer PUBLIC OpenGL::OpenGL OpenGL::EGL)
endif()

if(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  target_compile_definitions(raytracer PUBLIC RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  target_link_libraries(raytracer PUBLIC Vulkan::Vulkan)
endif()

if(RAYTRACER_ENABLE_STATS)
  # PUBLIC so headers like include/render/Stats.h see the same definition
  # in the library and in dependents (tests, tools, Modeler). Without that,
  # a translation unit could include Stats.h with the macro undefined and
  # silently emit no-op increments while the library counted them.
  target_compile_definitions(raytracer PUBLIC RAYTRACER_ENABLE_STATS)
endif()

# Make autogen depend on the expanded glob lists too. Otherwise an existing
# build tree can reconfigure after a pull but keep a stale automoc depfile that
# never parses newly added Q_OBJECT headers.
set_target_properties(raytracer PROPERTIES
  AUTOMOC ON
  AUTOGEN_TARGET_DEPENDS "${RAYTRACER_SOURCES};${RAYTRACER_HEADERS};${RAYTRACER_UIS}"
  AUTOUIC ON
  AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/widgets;${CMAKE_CURRENT_SOURCE_DIR}/widgets/world"
)
