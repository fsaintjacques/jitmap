set(SOURCES
  query/compiler.cc
  query/expr.cc
  query/jit.cc
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

# Required for the jit engine
set(LLVM_CORE_COMPONENTS support core irreader orcjit vectorize)
set(LLVM_NATIVE_JIT_COMPONENTS x86codegen x86asmparser x86disassembler x86desc x86info x86utils perfjitevents)

llvm_map_components_to_libnames(LLVM_LIBRARIES ${LLVM_CORE_COMPONENTS} ${LLVM_NATIVE_JIT_COMPONENTS})
target_link_libraries(jitmap ${LLVM_LIBRARIES})
