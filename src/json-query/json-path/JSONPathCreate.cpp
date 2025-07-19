#include <json-path/JSONPath.hpp>

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"

JSONPath::Result JSONPath::create(QStringView rawPath, Option opt)
{
    // Use the new JSONPathCompiler API
    auto result = json_query::json_path::compile(rawPath);
    if (!result)
        return std::unexpected(result.error());

    // Success: build the object using the compiled result
    return JSONPath(opt,
                    result->function,                    // function type from compiler
                    rawPath.toString(),                  // keep original as given
                    std::move(result->compiled.tokens),  // tokens from compiler
                    std::move(result->compiled.filters)); // filters from compiler
}

std::expected<json_query::json_path::Compiled, json_query::json_path::Error> compilePath(QStringView sv)
{
    auto result = json_query::json_path::compilePath(sv);
    if (!result)
        return std::unexpected(result.error());
    
    return json_query::json_path::Compiled{
        std::move(result->tokens),
        std::move(result->filters)
    };
}

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp
// We do NOT redefine it here to avoid infinite recursion with JSONPathCompiler