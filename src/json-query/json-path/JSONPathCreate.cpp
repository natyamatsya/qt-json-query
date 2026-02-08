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

    auto& tokens{compileResult->compiled.tokens};

    // Pre-compute whether the path is definite (only Key/Index selectors, no unions).
    // A single-token bracket group (e.g. [5]) is still definite; only multi-token
    // bracket groups (unions like [0,2]) disqualify.
    const auto definite{[&tokens]
                        {
                            for (std::size_t i{1}; i < tokens.size(); ++i)
                            {
                                const auto& tk{tokens[i]};
                                if (tk.kind != Token::Kind::Key && tk.kind != Token::Kind::Index)
                                    return false;
                                if (tk.bracketGroupId > 0 && i + 1 < tokens.size() &&
                                    tokens[i + 1].bracketGroupId == tk.bracketGroupId)
                                    return false;
                            }
                            return true;
                        }()};

    return JSONPath(compileResult->function, rawPath.toString(), std::move(tokens), definite);
}

} // namespace json_query::json_path

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp
