#include <json-path/JSONPath.hpp>

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"

namespace json_query {
    using json_path::compile;

    JSONPath::Result JSONPath::create(QStringView rawPath, Option opt)
    {
        auto result = compile(rawPath);
        if (!result.has_value())
            return std::unexpected(result.error());

        return JSONPath(opt,
                        result.value().function,
                        rawPath.toString(),
                        std::move(result.value().compiled.tokens),
                        std::move(result.value().compiled.filters));
    }

} // namespace json_query

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp