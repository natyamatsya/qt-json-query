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
        auto result = compile(rawPath);
        if (!result.has_value()) {
            qCDebug(jsonPathLog) << "JSONPath::create() compile failed, returning error" << static_cast<int>(result.error());
            return std::unexpected(result.error());
        }
        qCDebug(jsonPathLog) << "JSONPath::create() compile succeeded, creating JSONPath object";

        return JSONPath(result.value().function,
                        rawPath.toString(),
                        std::move(result.value().compiled.tokens),
                        std::move(result.value().compiled.filters),
                        std::move(result.value().compiled.contextFilters));
    }

} // namespace json_query

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp