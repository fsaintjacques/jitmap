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

unit_test(query_compiler_test SOURCES compiler_test.cc)
unit_test(query_expr_test SOURCES expr_test.cc)
unit_test(query_matcher_test SOURCES matcher_test.cc)
unit_test(query_optimizer_test SOURCES optimizer_test.cc)
unit_test(query_parser_test SOURCES parser_test.cc)
unit_test(query_query_test SOURCES query_test.cc)
