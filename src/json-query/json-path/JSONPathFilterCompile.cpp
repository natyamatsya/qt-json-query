// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathFilter.hpp"
#include "json-query/json-path/JSONPathFilterParsers.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

namespace json_query::json_path::detail
{

using json_query::json_path::FilterFn;
using json_query::json_path::Token;
using json_query::json_path::detail::stripOuterParens;

// Table-driven parser dispatch system
// Rules are ordered by precedence (lowest precedence first)
const std::array<RuleFn, 10> rules = {
    &parseOr, // lowest precedence first
    &parseAnd,
    &parseNot,          // Add negation parser with high precedence
    &parseAbsolutePath, // Add absolute path parser
    &parseIn,
    &parseExists,
    &parseSelfCmp,
    &parseFunction, // Add function call parser
    &parseCompare,
    &parseRegex // highest precedence
};

} // namespace json_query::json_path::detail

namespace json_query::json_path
{

// Main filter compilation orchestration
std::optional<Token> compileFilter(const QString& expr, std::vector<FilterFn>& out)
{
    auto s = detail::stripOuterParens(expr);
    qCDebug(jsonPathLog) << "compileFilter expr=" << expr << "stripped=" << s;

    for (int i = 0; i < detail::rules.size(); ++i)
    {
        const auto rule{detail::rules[i]};
        qCDebug(jsonPathLog) << "Trying rule" << i;

        if (auto result = rule(s, out))
        {
            qCDebug(jsonPathLog) << "compileFilter accepted token kind="
                                 << (result ? static_cast<int>(result->kind) : -1) << "from rule" << i;
            return result;
        }
    }

    return std::nullopt;
}

} // namespace json_query::json_path
