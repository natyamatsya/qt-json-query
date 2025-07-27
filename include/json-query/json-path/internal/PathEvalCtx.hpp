#pragma once

#include <vector>
#include <QString>
#include <QJsonValue>

#include "json-query/json-path/JSONPathCompile.hpp" // for Token, FunctionType, Slice
#include "json-query/json-path/JSONPathOption.hpp"

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
    using Token              = json_path::Token;
    using Function           = json_path::FunctionType;

    PathEvalCtx(const std::vector<Token>&   t,
                const QJsonValue& root,
                Function                 fn) noexcept
        : tokens{t}, rootDocument{root}, trailingFn{fn} {}

    // Data members are intentionally const refs – PathEvalCtx is just a view.
    const std::vector<Token>&   tokens;
    const QJsonValue& rootDocument;
    Function                 trailingFn {Function::None};
};

} // namespace json_query::json_path::detail
