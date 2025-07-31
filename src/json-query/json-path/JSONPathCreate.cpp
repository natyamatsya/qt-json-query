// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/utils/JSONQueryError.hpp"

namespace json_query::json_path
{

JSONPath::ParseResult JSONPath::create(QStringView rawPath)
{
    qCDebug(jsonPathLog) << "JSONPath::create() called with rawPath=" << rawPath;
    auto compileResult = compile(rawPath);

    if (!compileResult)
    {
        qCDebug(jsonPathLog) << "JSONPath::create() compile failed with error:"
                             << json_query::toQStringView(compileResult.error());
        return std::unexpected(QueryError{ErrorDomain::PathParse, static_cast<std::uint8_t>(compileResult.error())});
    }

    qCDebug(jsonPathLog) << "JSONPath::create() compile succeeded, creating JSONPath object";
    return JSONPath(compileResult->function, rawPath.toString(), std::move(compileResult->compiled.tokens));
}

} // namespace json_query::json_path

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp
