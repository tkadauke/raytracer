file(GLOB RENDERCLI_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
add_executable(rendercli ${RENDERCLI_SOURCES})
# WHOLE_ARCHIVE forces every object file from libraytracer.a into the link
# even if no symbol from it is referenced. Required because the world/sampler/
# camera/material/texture factories register themselves at static-init time
# (`static bool dummy = ...registerClass<Foo>("Foo")`) and the registration is
# only reached by string-keyed lookup at runtime — without WHOLE_ARCHIVE the
# linker dead-strips e.g. RandomSampler.cpp.o and SamplerFactory::create
# returns nullptr, which then crashes.
target_link_libraries(rendercli PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,raytracer>)
