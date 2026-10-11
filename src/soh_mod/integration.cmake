# Include after SoH's add_executable(soh ...), with STEVE_BRIDGE_ROOT set.
if(NOT TARGET soh)
  message(FATAL_ERROR "Include integration.cmake after the soh target is created")
endif()
if(MSVC)
  set(CMAKE_CXX26_STANDARD_COMPILE_OPTION "/std:c++latest")
  set(CMAKE_CXX26_EXTENSION_COMPILE_OPTION "/std:c++latest")
  string(REGEX REPLACE "[-/]std:c\\+\\+20" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_STANDARD 26)
  set_target_properties(soh PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED YES)
  # MSVC LTCG serializes large intermediate ASTs; keep /O2 within the disk budget.
  set_target_properties(soh PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
  # LUS exports its props file through the cache, overriding the root default.
  # Set the actual target properties consumed by its generator expressions.
  set_target_properties(soh libultraship PROPERTIES
    DEFAULT_CXX_DEBUG_INFORMATION_FORMAT_RELEASE ""
    MSVC_DEBUG_INFORMATION_FORMAT "")
  target_compile_options(soh PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/Zc:inline>
    $<$<COMPILE_LANGUAGE:CXX>:/Gw>)
endif()
add_subdirectory("${STEVE_BRIDGE_ROOT}" "${CMAKE_BINARY_DIR}/steve-bridge")
target_sources(soh PRIVATE "${STEVE_BRIDGE_ROOT}/src/soh_mod/SteveActor.c")
target_include_directories(soh PRIVATE "${STEVE_BRIDGE_ROOT}/src")
target_link_libraries(soh PRIVATE bridge_core)
