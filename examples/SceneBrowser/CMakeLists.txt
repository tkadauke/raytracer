file(GLOB SCENE_BROWSER_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
)

add_executable(SceneBrowser ${SCENE_BROWSER_SOURCES})
set_target_properties(SceneBrowser PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
)
target_link_libraries(SceneBrowser PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,raytracer>)
