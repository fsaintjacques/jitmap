function(tool tool_name)
  set(options)
  set(one_value_args)
  set(multi_value_args SOURCES)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if (ARG_SOURCES)
    set(SOURCES ${ARG_SOURCES})
  else()
    set(SOURCES ${tool_name}.cc)
  endif()

  add_executable(${tool_name} ${SOURCES})
  target_link_libraries(${tool_name} jitmap)
  target_compile_options(${tool_name} PRIVATE ${CXX_WARNING_FLAGS})
  target_include_directories(${tool_name} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    )
endfunction()

tool(jitmap-ir SOURCES jitmap_ir.cc)
