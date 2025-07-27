#include <json-path/JSONPath.hpp>

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

namespace json_query {
    using json_path::compile;
    using json_path::jsonPathLog;

    JSONPath::Result JSONPath::create(QStringView rawPath)
    {
        qCDebug(jsonPathLog) << "JSONPath::create() called with rawPath=" << rawPath;
        
        // C++23 Monadic Chain - Elegant error composition without manual checks!
        return compile(rawPath)
            .and_then([&](json_path::CompilationResult compilationResult) -> JSONPath::Result {
                qCDebug(jsonPathLog) << "JSONPath::create() compile succeeded, creating JSONPath object";
                
                return JSONPath(compilationResult.function,
                            rawPath.toString(),
                            std::move(compilationResult.compiled.tokens));
            })
            .transform_error([&](json_path::Error error) -> Error {
                qCDebug(jsonPathLog) << "JSONPath::create() compile failed with error:" << json_path::toString(error).data();
                return error;
            });
    }

} // namespace json_query

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp