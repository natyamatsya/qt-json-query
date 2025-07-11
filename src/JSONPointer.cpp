// jsonpointer.cpp - Using CTRE
#include "json-query/JSONPointer.hpp"
#include "json-query/JSONQueryUtils.hpp"


// ----------------
// CTRE Patterns
// ----------------
namespace
{
    using namespace json_utils;

    // Match each path segment between slashes (at least one non-slash char)
    constexpr auto token_pattern = ctll::fixed_string{
        "/([^/]+)"};

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

    // Manual single-pass scan to extract tokens without temporary QStringList
    QStringView view{pointer};
    qsizetype start = 1; // skip leading '/'
    const qsizetype n = view.size();
    while (start <= n)
    {
        qsizetype pos = start;
        while (pos < n && view.at(pos) != u'/')
            ++pos;
        // Extract token substring [start, pos)
        if (pos > start) // ignore empty segments (consecutive slashes)
        {
            QString token = decodeToken(QString(view.sliced(start, pos - start)));
            m_tokens.append(std::move(token));
        }
        start = pos + 1; // move past '/'
    }
}

QString JSONPointer::decodeToken(const QString &token)
{
    // Fast single-pass decoder for RFC-6901 escape sequences ("~0" → "~", "~1" → "/").
    // Avoids conversions to std::string and extra allocations.

    if (!token.contains(u'~'))
        return token; // early exit when no escapes present

    QString out;
    out.reserve(token.size()); // worst-case same length

    const QStringView view{token};
    for (qsizetype i = 0; i < view.size(); ++i)
    {
        const QChar ch = view.at(i);
        if (ch != u'~' || i + 1 >= view.size())
        {
            out += ch;
            continue;
        }

        const QChar next = view.at(i + 1);
        if (next == u'1')
        {
            out += u'/';
            ++i; // skip second char
        }
        else if (next == u'0')
        {
            out += u'~';
            ++i;
        }
        else
        {
            out += ch; // unknown escape, keep '~'
        }
    }
    return out;
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
