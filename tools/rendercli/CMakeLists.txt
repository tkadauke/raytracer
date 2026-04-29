file(GLOB RENDERCLI_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
add_executable(rendercli ${RENDERCLI_SOURCES})
target_link_libraries(rendercli PRIVATE raytracer)
