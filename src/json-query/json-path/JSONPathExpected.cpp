#include "json-query/json-path/JSONPathExpected.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp" // normalizeIndex

namespace json_query::json_path::detail
{

std::expected<QJsonValue, EvalError> evaluateDefinite(const std::vector<Token>& tokens,
                                                      const QJsonValue&         root) noexcept
{
    using enum Token::Kind;

    auto cur{root};
    // Skip leading root token ('$' or '@') if present
    auto startIdx{0};
    if (!tokens.empty() && tokens.front().kind == Token::Kind::Key)
    {
        const auto& k = tokens.front().key;
        if (k == u"$" || k == u"@")
            startIdx = 1;
    }

    for (int i = startIdx; i < tokens.size(); ++i)
    {
        const auto& tk = tokens[i];
        switch (tk.kind)
        {
        case Key:
        {
            if (!cur.isObject())
                return std::unexpected(EvalError::TypeMismatchObject);
            const auto obj{cur.toObject()};
            auto       it{obj.constFind(tk.key)};
            if (it == obj.constEnd())
                return std::unexpected(EvalError::KeyNotFound);
            cur = *it;
            break;
        }
        case Index:
        {
            if (!cur.isArray())
                return std::unexpected(EvalError::TypeMismatchArray);
            const auto arr{cur.toArray()};
            auto       idx = normalizeIndex(tk.index, arr.size());
            if (idx < 0 || idx >= arr.size())
                return std::unexpected(EvalError::IndexOutOfRange);
            cur = arr[idx];
            break;
        }
        default:
            // Unsupported token kinds for definite evaluation path
            return std::unexpected(EvalError::TypeMismatchObject);
        }
    }
    return cur;
}

} // namespace json_query::json_path::detail
