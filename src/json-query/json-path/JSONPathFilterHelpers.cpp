// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathFilterHelpers.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"

namespace json_query::json_path::detail
{

using json_query::json_path::FilterFn;
using json_query::json_path::Token;
using json_query::utils::to_qstr;
using json_query::utils::to_sv;

// Helper function for JSON number validation with strict JSON compliance
[[nodiscard]] bool isValidJsonNumber(const QString& value) noexcept
{
    if (value.isEmpty())
        return false;

    auto       pos{0};
    const auto len{value.length()};

    // Handle optional minus sign
    if (pos < len && value[pos] == '-')
        pos++;

    // Must have at least one digit after optional minus
    if (pos >= len || !value[pos].isDigit())
        return false;

    // Handle integer part - either '0' or non-zero digit followed by digits
    if (value[pos] == '0')
    {
        pos++; // Single zero
        // Leading zeros not allowed (e.g., "00", "01" are invalid)
        if (pos < len && value[pos].isDigit())
            return false;
    }
    else
    {
        // Non-zero digit followed by optional digits
        while (pos < len && value[pos].isDigit())
            pos++;
    }

    // Handle optional fractional part
    if (pos < len && value[pos] == '.')
    {
        pos++;
        // Must have at least one digit after decimal point
        if (pos >= len || !value[pos].isDigit())
            return false;
        // Continue reading digits
        while (pos < len && value[pos].isDigit())
            pos++;
    }

    // Handle optional exponent part
    if (pos < len && (value[pos] == 'e' || value[pos] == 'E'))
    {
        pos++;
        // Optional plus or minus sign
        if (pos < len && (value[pos] == '+' || value[pos] == '-'))
            pos++;
        // Must have at least one digit
        if (pos >= len || !value[pos].isDigit())
            return false;
        // Continue reading digits
        while (pos < len && value[pos].isDigit())
            pos++;
    }

    // Should have consumed the entire string
    return pos == len;
}

// Helper function for unquoting strings
bool unquote(QString& s)
{
    if (s.length() < 2)
        return false;

    const QChar first = s[0];
    const QChar last  = s[s.length() - 1];

    if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
    {
        s = s.mid(1, s.length() - 2);

        // Process escape sequences
        QString result;
        result.reserve(s.length());

        for (int i = 0; i < s.length(); ++i)
        {
            if (s[i] == '\\' && i + 1 < s.length())
            {
                const QChar next = s[i + 1];
                switch (next.unicode())
                {
                case '"':
                    result += '"';
                    break;
                case '\'':
                    result += '\'';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '/':
                    result += '/';
                    break;
                case 'b':
                    result += '\b';
                    break;
                case 'f':
                    result += '\f';
                    break;
                case 'n':
                    result += '\n';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case 't':
                    result += '\t';
                    break;
                case 'u':
                    // Unicode escape sequence \uXXXX
                    if (i + 5 < s.length())
                    {
                        bool         ok;
                        const auto   hex{s.mid(i + 2, 4)};
                        const ushort code = hex.toUShort(&ok, 16);
                        if (ok)
                        {
                            result += QChar(code);
                            i += 4; // Skip the 4 hex digits
                        }
                        else
                        {
                            return false; // Invalid unicode escape
                        }
                    }
                    else
                    {
                        return false; // Incomplete unicode escape
                    }
                    break;
                default:
                    return false; // Invalid escape sequence
                }
                ++i; // Skip the escape character
            }
            else
            {
                result += s[i];
            }
        }

        s = result;
        return true;
    }

    return false;
}

// Helper function to strip outer parentheses from expressions
QString stripOuterParens(QString s)
{
    s = s.trimmed();
    while (s.startsWith('(') && s.endsWith(')'))
    {
        // Check if these are truly outer parentheses by counting
        auto depth{0};
        auto isOuter{true};
        for (int i = 1; i < s.length() - 1; ++i)
        {
            if (s[i] == '(')
                depth++;
            else if (s[i] == ')')
            {
                depth--;
                if (depth < 0)
                {
                    isOuter = false;
                    break;
                }
            }
        }
        if (isOuter && depth == 0)
            s = s.mid(1, s.length() - 2).trimmed();
        else
            break;
    }
    return s;
}

// Builder implementation
Token Builder::add(FilterFn fn, QString key)
{
    fns.push_back(std::move(fn));
    const auto id{fns.size() - 1};

    // Create token with proper field initialization
    Token token;
    token.kind            = Token::Kind::Filter;
    token.index           = 0;
    token.slice           = {};
    token.hash            = 0u;
    token.key             = std::move(key);
    token.filterId        = id;
    token.contextFilterId = SIZE_MAX; // Not using context filter
    token.bracketGroupId  = -1;

    return token;
}

} // namespace json_query::json_path::detail
