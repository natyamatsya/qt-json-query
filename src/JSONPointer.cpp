// jsonpointer.cpp - Using CTRE
#include "json-query/JSONPointer.h"
#include "json-query/json_query_utils.h"

// ----------------
// CTRE Patterns
// ----------------
namespace
{
    using namespace json_utils;

    constexpr auto token_pattern = ctll::fixed_string{
        "/([^/]*)"};

    constexpr auto escape_tilde_pattern = ctll::fixed_string{
        "~0"};

    constexpr auto escape_slash_pattern = ctll::fixed_string{
        "~1"};
}

JSONPointer::JSONPointer(const QString &pointer)
{
    parsePointer(pointer);
}

void JSONPointer::parsePointer(const QString &pointer)
{
    using namespace json_utils;

    // Empty pointer is valid and represents the entire document
    if (pointer.isEmpty())
    {
        return;
    }

    // According to RFC 6901, a JSON Pointer must start with '/'
    if (!pointer.startsWith('/'))
    {
        m_valid = false;
        return;
    }

    // Find all tokens using CTRE
    std::string_view sv = to_sv(pointer);
    auto tokenMatches = ctre::range<token_pattern>(sv);

    for (auto match : tokenMatches)
    {
        if (match.get<1>())
        {
            QString token = to_qstr(match.get<1>().to_view());
            m_tokens.append(decodeToken(token));
        }
    }
}

QString JSONPointer::decodeToken(const QString &token)
{
    using namespace json_utils;

    // According to RFC 6901, '~1' is used to represent '/' and '~0' is used to represent '~'
    // Apply the replacements using CTRE
    std::string_view sv = to_sv(token);
    std::string result(sv);

    // First replace ~1 with / (must be done first to avoid ambiguity)
    auto slashMatches = ctre::range<escape_slash_pattern>(sv);
    for (auto it = slashMatches.rbegin(); it != slashMatches.rend(); ++it)
    {
        auto &match = *it;
        size_t pos = match.get<0>().begin() - sv.begin();
        result.replace(pos, 2, "/");
    }

    // Then replace ~0 with ~
    std::string_view resultSv(result);
    auto tildeMatches = ctre::range<escape_tilde_pattern>(resultSv);
    for (auto it = tildeMatches.rbegin(); it != tildeMatches.rend(); ++it)
    {
        auto &match = *it;
        size_t pos = match.get<0>().begin() - resultSv.begin();
        result.replace(pos, 2, "~");
    }

    return QString::fromStdString(result);
}

QJsonValue JSONPointer::evaluate(const QJsonDocument &document) const
{
    return evaluate(document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array()));
}

QJsonValue JSONPointer::evaluate(const QJsonValue &value) const
{
    if (!isValid())
    {
        return QJsonValue(QJsonValue::Undefined);
    }

    // Empty path returns the value itself
    if (m_tokens.isEmpty())
    {
        return value;
    }

    return evaluateInternal(value, 0);
}

QJsonValue JSONPointer::evaluateInternal(const QJsonValue &value, int tokenIndex) const
{
    if (tokenIndex >= m_tokens.size())
    {
        return value;
    }

    const QString &token = m_tokens.at(tokenIndex);

    if (value.isObject())
    {
        QJsonObject obj = value.toObject();
        if (!obj.contains(token))
        {
            return QJsonValue(QJsonValue::Undefined);
        }
        return evaluateInternal(obj.value(token), tokenIndex + 1);
    }
    else if (value.isArray())
    {
        QJsonArray array = value.toArray();
        bool ok = false;
        int index = token.toInt(&ok);

        // Check if the token is a valid array index
        if (!ok || index < 0 || index >= array.size())
        {
            return QJsonValue(QJsonValue::Undefined);
        }

        return evaluateInternal(array.at(index), tokenIndex + 1);
    }

    // If we're trying to navigate further but current value is not an object or array
    return QJsonValue(QJsonValue::Undefined);
}

bool JSONPointer::isValid() const
{
    return m_valid;
}

QString JSONPointer::toString() const
{
    using namespace json_utils;

    if (!isValid())
    {
        return QString();
    }

    // Special case for an empty path
    if (m_tokens.isEmpty())
    {
        return QString();
    }

    // Build the string representation
    QString result;
    for (const QString &token : m_tokens)
    {
        // Encode special characters using CTRE
        std::string_view sv = to_sv(token);
        std::string encodedToken(sv);

        // First replace ~ with ~0
        for (size_t pos = 0; pos < encodedToken.size(); ++pos)
        {
            if (encodedToken[pos] == '~')
            {
                encodedToken.replace(pos, 1, "~0");
                pos += 1; // Skip the replacement
            }
        }

        // Then replace / with ~1
        for (size_t pos = 0; pos < encodedToken.size(); ++pos)
        {
            if (encodedToken[pos] == '/')
            {
                encodedToken.replace(pos, 1, "~1");
                pos += 1; // Skip the replacement
            }
        }

        result += "/" + QString::fromStdString(encodedToken);
    }

    return result;
}
