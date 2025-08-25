// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/utils/JSONQueryError.hpp"

namespace json_query::json_path
{

JSONPath::ParseResult JSONPath::create(QStringView rawPath)
{
    qCDebug(jsonPathLog) << "DEBUG: JSONPath::create() called with rawPath=" << rawPath;
    qCDebug(jsonPathLog) << "JSONPath::create() called with rawPath=" << rawPath;
    auto compileResult = compile(rawPath);

    if (!compileResult)
    {
        qCDebug(jsonPathLog) << "DEBUG: JSONPath::create() compile FAILED with error:"
                             << to_qt_sv(compileResult.error());
        qCDebug(jsonPathLog) << "JSONPath::create() compile failed with error:" << to_qt_sv(compileResult.error());
        return std::unexpected(QueryError{ErrorDomain::PathParse, static_cast<std::uint8_t>(compileResult.error())});
    }

    qCDebug(jsonPathLog) << "DEBUG: JSONPath::create() compile SUCCEEDED, creating JSONPath with"
                         << compileResult->compiled.tokens.size() << "tokens";
    for (size_t i = 0; i < compileResult->compiled.tokens.size(); ++i)
    {
        qCDebug(jsonPathLog) << "DEBUG: Token[" << i
                             << "] kind=" << static_cast<int>(compileResult->compiled.tokens[i].kind)
                             << "key=" << compileResult->compiled.tokens[i].key;
    }
    qCDebug(jsonPathLog) << "JSONPath::create() compile succeeded, creating JSONPath object";
    return JSONPath(compileResult->function, rawPath.toString(), std::move(compileResult->compiled.tokens));
}

} // namespace json_query::json_path

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp
