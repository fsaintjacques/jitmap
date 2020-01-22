# Copyright 2020 RStudio, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

# Required for the jit engine
set(LLVM_CORE_COMPONENTS support core irreader orcjit vectorize)
set(LLVM_NATIVE_JIT_COMPONENTS x86codegen x86asmparser x86disassembler x86desc x86info x86utils perfjitevents)

llvm_map_components_to_libnames(LLVM_LIBRARIES ${LLVM_CORE_COMPONENTS} ${LLVM_NATIVE_JIT_COMPONENTS})
target_link_libraries(jitmap ${LLVM_LIBRARIES})
