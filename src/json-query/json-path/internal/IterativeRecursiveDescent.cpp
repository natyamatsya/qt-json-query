// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/internal/IterativeRecursiveDescent.hpp"

namespace json_query::json_path::internal
{

// Thread-local statistics definition - moved to .cpp to avoid duplicate symbols
thread_local IterativeRecursiveDescent::Stats IterativeRecursiveDescent::stats_;

} // namespace json_query::json_path::internal
