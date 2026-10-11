# Include after SoH's add_executable(soh ...), with STEVE_BRIDGE_ROOT set.
if(NOT TARGET soh)
  message(FATAL_ERROR "Include integration.cmake after the soh target is created")
endif()
add_subdirectory("${STEVE_BRIDGE_ROOT}" "${CMAKE_BINARY_DIR}/steve-bridge")
target_sources(soh PRIVATE "${STEVE_BRIDGE_ROOT}/src/soh_mod/SteveActor.c")
target_include_directories(soh PRIVATE "${STEVE_BRIDGE_ROOT}/src")
target_link_libraries(soh PRIVATE bridge_core)
