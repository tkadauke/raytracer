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

if(RAYTRACER_BUILD_BENCHMARKS AND
   NOT RAYTRACER_BUILD_MODELER AND
   NOT RAYTRACER_BUILD_TESTS AND
   NOT RAYTRACER_BUILD_TOOLS)
  list(FILTER RAYTRACER_SOURCES EXCLUDE REGEX "/(widgets|world)/")
  list(FILTER RAYTRACER_HEADERS EXCLUDE REGEX "/include/(widgets|world)/")
  list(FILTER RAYTRACER_UIS EXCLUDE REGEX "/widgets/")
endif()

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
  target_sources(raytracer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalGpuDiffusePathFrontierCompactionBackend.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalResidentPathCompactionBackend.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalTracingAccumulationKernel.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalWavefrontSmokeKernel.mm"
  )
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalGpuDiffusePathFrontierCompactionBackend.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalResidentPathCompactionBackend.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/MetalTracingAccumulationKernel.mm"
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
  set(RAYTRACER_VULKAN_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/render/shaders")
  set(RAYTRACER_VULKAN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/render")
  set(RAYTRACER_VULKAN_SMOKE_SHADER
      "${RAYTRACER_VULKAN_SHADER_DIR}/vulkan_wavefront_smoke.comp")
  set(RAYTRACER_VULKAN_SMOKE_SPIRV
      "${RAYTRACER_VULKAN_GENERATED_DIR}/vulkan_wavefront_smoke.spv")
  set(RAYTRACER_VULKAN_SHADER_HEADER
      "${RAYTRACER_VULKAN_GENERATED_DIR}/VulkanWavefrontShaders.generated.h")
  set(RAYTRACER_VULKAN_TRIANGLE_CLOSEST_SHADER
      "${RAYTRACER_VULKAN_SHADER_DIR}/vulkan_wavefront_triangle_closest.comp")
  set(RAYTRACER_VULKAN_TRIANGLE_CLOSEST_SPIRV
      "${RAYTRACER_VULKAN_GENERATED_DIR}/vulkan_wavefront_triangle_closest.spv")
  set(RAYTRACER_VULKAN_TRIANGLE_CLOSEST_HEADER
      "${RAYTRACER_VULKAN_GENERATED_DIR}/VulkanWavefrontTriangleClosest.generated.h")
  set(RAYTRACER_VULKAN_TRIANGLE_ANY_SHADER
      "${RAYTRACER_VULKAN_SHADER_DIR}/vulkan_wavefront_triangle_any.comp")
  set(RAYTRACER_VULKAN_TRIANGLE_ANY_SPIRV
      "${RAYTRACER_VULKAN_GENERATED_DIR}/vulkan_wavefront_triangle_any.spv")
  set(RAYTRACER_VULKAN_TRIANGLE_ANY_HEADER
      "${RAYTRACER_VULKAN_GENERATED_DIR}/VulkanWavefrontTriangleAny.generated.h")
  set(RAYTRACER_VULKAN_RAY_COMPACTION_SHADER
      "${RAYTRACER_VULKAN_SHADER_DIR}/vulkan_wavefront_ray_compaction.comp")
  set(RAYTRACER_VULKAN_RAY_COMPACTION_SPIRV
      "${RAYTRACER_VULKAN_GENERATED_DIR}/vulkan_wavefront_ray_compaction.spv")
  set(RAYTRACER_VULKAN_RAY_COMPACTION_HEADER
      "${RAYTRACER_VULKAN_GENERATED_DIR}/VulkanWavefrontRayCompaction.generated.h")
  set(RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_SHADER
      "${RAYTRACER_VULKAN_SHADER_DIR}/vulkan_diffuse_path_frontier_compaction.comp")
  set(RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_SPIRV
      "${RAYTRACER_VULKAN_GENERATED_DIR}/vulkan_diffuse_path_frontier_compaction.spv")
  set(RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_HEADER
      "${RAYTRACER_VULKAN_GENERATED_DIR}/VulkanDiffusePathFrontierCompaction.generated.h")
  set(RAYTRACER_VULKAN_ACCUMULATION_CLEAR_SHADER
      "${RAYTRACER_VULKAN_SHADER_DIR}/vulkan_tracing_accumulation_clear.comp")
  set(RAYTRACER_VULKAN_ACCUMULATION_CLEAR_SPIRV
      "${RAYTRACER_VULKAN_GENERATED_DIR}/vulkan_tracing_accumulation_clear.spv")
  set(RAYTRACER_VULKAN_ACCUMULATION_CLEAR_HEADER
      "${RAYTRACER_VULKAN_GENERATED_DIR}/VulkanTracingAccumulationClear.generated.h")
  set(RAYTRACER_VULKAN_ACCUMULATION_ADD_SHADER
      "${RAYTRACER_VULKAN_SHADER_DIR}/vulkan_tracing_accumulation_add.comp")
  set(RAYTRACER_VULKAN_ACCUMULATION_ADD_SPIRV
      "${RAYTRACER_VULKAN_GENERATED_DIR}/vulkan_tracing_accumulation_add.spv")
  set(RAYTRACER_VULKAN_ACCUMULATION_ADD_HEADER
      "${RAYTRACER_VULKAN_GENERATED_DIR}/VulkanTracingAccumulationAdd.generated.h")
  set(RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_SHADER
      "${RAYTRACER_VULKAN_SHADER_DIR}/vulkan_tracing_accumulation_resolve.comp")
  set(RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_SPIRV
      "${RAYTRACER_VULKAN_GENERATED_DIR}/vulkan_tracing_accumulation_resolve.spv")
  set(RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_HEADER
      "${RAYTRACER_VULKAN_GENERATED_DIR}/VulkanTracingAccumulationResolve.generated.h")

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_SMOKE_SPIRV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RAYTRACER_VULKAN_GENERATED_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -O -fshader-stage=compute
            "${RAYTRACER_VULKAN_SMOKE_SHADER}" -o "${RAYTRACER_VULKAN_SMOKE_SPIRV}"
    DEPENDS "${RAYTRACER_VULKAN_SMOKE_SHADER}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_SHADER_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${RAYTRACER_VULKAN_SMOKE_SPIRV}
            -DOUTPUT=${RAYTRACER_VULKAN_SHADER_HEADER}
            -DSYMBOL=smokeHitMissShaderSpirv
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${RAYTRACER_VULKAN_SMOKE_SPIRV}"
            "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_SPIRV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RAYTRACER_VULKAN_GENERATED_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -O -fshader-stage=compute
            "${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_SHADER}"
            -o "${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_SPIRV}"
    DEPENDS "${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_SHADER}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_SPIRV}
            -DOUTPUT=${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_HEADER}
            -DSYMBOL=triangleClosestHitShaderSpirv
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_SPIRV}"
            "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_TRIANGLE_ANY_SPIRV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RAYTRACER_VULKAN_GENERATED_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -O -fshader-stage=compute
            "${RAYTRACER_VULKAN_TRIANGLE_ANY_SHADER}"
            -o "${RAYTRACER_VULKAN_TRIANGLE_ANY_SPIRV}"
    DEPENDS "${RAYTRACER_VULKAN_TRIANGLE_ANY_SHADER}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_TRIANGLE_ANY_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${RAYTRACER_VULKAN_TRIANGLE_ANY_SPIRV}
            -DOUTPUT=${RAYTRACER_VULKAN_TRIANGLE_ANY_HEADER}
            -DSYMBOL=triangleAnyHitShaderSpirv
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${RAYTRACER_VULKAN_TRIANGLE_ANY_SPIRV}"
            "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_RAY_COMPACTION_SPIRV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RAYTRACER_VULKAN_GENERATED_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -O -fshader-stage=compute
            "${RAYTRACER_VULKAN_RAY_COMPACTION_SHADER}"
            -o "${RAYTRACER_VULKAN_RAY_COMPACTION_SPIRV}"
    DEPENDS "${RAYTRACER_VULKAN_RAY_COMPACTION_SHADER}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_RAY_COMPACTION_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${RAYTRACER_VULKAN_RAY_COMPACTION_SPIRV}
            -DOUTPUT=${RAYTRACER_VULKAN_RAY_COMPACTION_HEADER}
            -DSYMBOL=rayCompactionShaderSpirv
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${RAYTRACER_VULKAN_RAY_COMPACTION_SPIRV}"
            "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_SPIRV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RAYTRACER_VULKAN_GENERATED_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -O -fshader-stage=compute
            "${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_SHADER}"
            -o "${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_SPIRV}"
    DEPENDS "${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_SHADER}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_SPIRV}
            -DOUTPUT=${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_HEADER}
            -DSYMBOL=diffusePathFrontierCompactionShaderSpirv
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_SPIRV}"
            "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_SPIRV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RAYTRACER_VULKAN_GENERATED_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -O -fshader-stage=compute
            "${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_SHADER}"
            -o "${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_SPIRV}"
    DEPENDS "${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_SHADER}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_SPIRV}
            -DOUTPUT=${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_HEADER}
            -DSYMBOL=tracingAccumulationClearShaderSpirv
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_SPIRV}"
            "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_ACCUMULATION_ADD_SPIRV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RAYTRACER_VULKAN_GENERATED_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -O -fshader-stage=compute
            "${RAYTRACER_VULKAN_ACCUMULATION_ADD_SHADER}"
            -o "${RAYTRACER_VULKAN_ACCUMULATION_ADD_SPIRV}"
    DEPENDS "${RAYTRACER_VULKAN_ACCUMULATION_ADD_SHADER}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_ACCUMULATION_ADD_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${RAYTRACER_VULKAN_ACCUMULATION_ADD_SPIRV}
            -DOUTPUT=${RAYTRACER_VULKAN_ACCUMULATION_ADD_HEADER}
            -DSYMBOL=tracingAccumulationAddShaderSpirv
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${RAYTRACER_VULKAN_ACCUMULATION_ADD_SPIRV}"
            "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_SPIRV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RAYTRACER_VULKAN_GENERATED_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -O -fshader-stage=compute
            "${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_SHADER}"
            -o "${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_SPIRV}"
    DEPENDS "${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_SHADER}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_SPIRV}
            -DOUTPUT=${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_HEADER}
            -DSYMBOL=tracingAccumulationResolveShaderSpirv
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    DEPENDS "${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_SPIRV}"
            "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
    VERBATIM
  )

  add_custom_target(raytracer_vulkan_wavefront_shaders
    DEPENDS
      "${RAYTRACER_VULKAN_SHADER_HEADER}"
      "${RAYTRACER_VULKAN_TRIANGLE_CLOSEST_HEADER}"
      "${RAYTRACER_VULKAN_TRIANGLE_ANY_HEADER}"
      "${RAYTRACER_VULKAN_RAY_COMPACTION_HEADER}"
      "${RAYTRACER_VULKAN_DIFFUSE_FRONTIER_COMPACTION_HEADER}"
      "${RAYTRACER_VULKAN_ACCUMULATION_CLEAR_HEADER}"
      "${RAYTRACER_VULKAN_ACCUMULATION_ADD_HEADER}"
      "${RAYTRACER_VULKAN_ACCUMULATION_RESOLVE_HEADER}"
  )

  target_compile_definitions(raytracer PUBLIC RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
  target_include_directories(raytracer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_link_libraries(raytracer PUBLIC Vulkan::Vulkan)
  add_dependencies(raytracer raytracer_vulkan_wavefront_shaders)
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
