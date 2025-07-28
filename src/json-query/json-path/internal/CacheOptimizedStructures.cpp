// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/internal/CacheOptimizedStructures.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace json_query::json_path::detail
{

// Thread-local pool definition - must be in a single translation unit to avoid duplicate symbols
thread_local StackFramePool CacheOptimizedStack::pool_;

} // namespace json_query::json_path::detail
