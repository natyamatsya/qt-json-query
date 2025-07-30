// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <json-path/JSONPath.hpp>

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/utils/JSONQueryError.hpp"

namespace json_query
{
using json_path::compile;
using json_path::jsonPathLog;

JSONPath::Result JSONPath::create(QStringView rawPath)
{
    qCDebug(jsonPathLog) << "JSONPath::create() called with rawPath=" << rawPath;

    // C++23 Monadic Chain - Elegant error composition with unified QueryError
    return compile(rawPath)
        .and_then(
            [&](json_path::CompilationResult compilationResult) -> JSONPath::Result
            {
                qCDebug(jsonPathLog) << "JSONPath::create() compile succeeded, creating JSONPath object";

                return JSONPath(
                    compilationResult.function, rawPath.toString(), std::move(compilationResult.compiled.tokens));
            })
        .transform_error(
            [&](json_path::ParseError error) -> JSONPath::Result::error_type
            {
                qCDebug(jsonPathLog) << "JSONPath::create() compile failed with error:"
                                     << json_path::to_string(error).data();
                return QueryError{ErrorDomain::PathParse, static_cast<std::uint8_t>(error)};
            });
}

} // namespace json_query

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp
