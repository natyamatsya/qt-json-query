// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/utils/JSONError.hpp"

namespace json_query::json_path
{

JSONPath::ParseResult JSONPath::create(QStringView rawPath)
{
    auto compileResult{compile(rawPath)};
    if (!compileResult)
        return std::unexpected(Error{ErrorDomain::PathParse, static_cast<std::uint8_t>(compileResult.error())});

    return JSONPath(compileResult->function, rawPath.toString(), std::move(compileResult->compiled.tokens));
}

} // namespace json_query::json_path

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp
