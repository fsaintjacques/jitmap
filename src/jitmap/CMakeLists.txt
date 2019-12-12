set(SOURCES
  query/compiler.cc
  query/expr.cc
  query/matcher.cc
  query/optimizer.cc
  query/parser.cc
  query/query.cc
  )

add_library(jitmap ${SOURCES})
target_include_directories(jitmap PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  )
target_compile_options(jitmap PRIVATE ${CXX_WARNING_FLAGS})
target_include_directories(jitmap PUBLIC ${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(LLVM_LIBRARIES support core irreader)
target_link_libraries(jitmap ${LLVM_LIBRARIES})
