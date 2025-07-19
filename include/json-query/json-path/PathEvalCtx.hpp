#pragma once

#include <QVector>
#include <QString>

#include "json-query/json-path/JSONPathCompile.hpp" // for Token, FunctionType, Slice
#include "json-query/json-path/JSONPath.hpp"        // for Option

namespace json_query::json_path::detail {

// A lightweight, immutable context object passed to pure evaluation helpers.
// It bundles the token stream and filter list plus the few high-level options
// normally stored inside the JSONPath instance.  No behaviour – just data.
//
// Having a separate context allows the evaluation layer to be unit-tested
// independently from the parsing/compilation layer.  Tests can construct a
// PathEvalCtx directly with hand-crafted token/filter arrays.
class PathEvalCtx
{
public:
    using Token      = json_path::Token;
    using FilterFn   = json_path::FilterFn;
    using Function   = json_path::FunctionType;
    using Option     = json_query::JSONPath::Option;

    PathEvalCtx(const QVector<Token>&   t,
                const QVector<FilterFn>& f,
                Option                   opt,
                Function                 fn) noexcept
        : tokens{t}, filters{f}, option{opt}, trailingFn{fn} {}

    // Data members are intentionally const refs – PathEvalCtx is just a view.
    const QVector<Token>&   tokens;
    const QVector<FilterFn>& filters;
    Option                   option {Option::None};
    Function                 trailingFn {Function::None};
};

} // namespace json_query::json_path::detail
