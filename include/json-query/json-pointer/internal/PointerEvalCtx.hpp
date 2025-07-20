#pragma once

#include <QVector>

#include "json-query/json-pointer/JSONPointerParsing.hpp" // for Token

namespace json_query::json_pointer::detail {

// Lightweight immutable view over a JSON Pointer token stream, passed
// to pure evaluation helpers to keep their signatures symmetrical with
// JSONPath's evaluation layer (which uses PathEvalCtx).
class PointerEvalCtx
{
public:
    using Token = json_query::json_pointer::detail::Token;

    explicit PointerEvalCtx(const QVector<Token>& t) noexcept : tokens{t} {}

    // Data member is a reference – context is just a view.
    const QVector<Token>& tokens;
};

} // namespace json_query::json_pointer::detail
