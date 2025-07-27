#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include "json-query/json-path/JSONPathCompile.hpp"

namespace json_query::json_path::detail
{

// Escapes a JSON Pointer segment according to RFC 6901 (~ -> ~0, / -> ~1)
QString escapePointerSegment(const QString& seg);

// Build a JSON Pointer string (without leading '/') from the token list starting at index 1
// Returns empty string if any token kind is unsupported for pointer conversion.
QString tokensToPointer(QStringList& segments, const std::vector<Token>& tokens);

// Convenience: directly build pointer from tokens list (skipping root)
inline QString tokensToPointer(const std::vector<Token>& tokens)
{
    QStringList segs;
    return tokensToPointer(segs, tokens);
}

} // namespace json_query::json_path::detail
