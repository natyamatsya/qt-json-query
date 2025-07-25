#pragma once

// Small collection of inline helper utilities shared between JSONPath
// compilation units (splitTopLevel, stripOuterParens).  Keeping them in
// a header avoids code duplication errors and allows multiple TUs to
// reference them.

#include <QString>
#include <QStringView>
#include <QLatin1String>
#include <optional>
#include <utility>
#include <QVector>

namespace json_query::json_path::detail {

// Remove exactly one balanced pair of outer parentheses, then trim.
inline QString stripOuterParens(QStringView sv)
{
    if (sv.size() >= 2 && sv.front() == u'(' && sv.back() == u')')
        sv = sv.mid(1, sv.size() - 2);
    return QString(sv).trimmed();
}

// Split at the first occurrence of `delim` that is *not* inside parentheses.
// Returns {left, right} if found; otherwise std::nullopt.
inline std::optional<std::pair<QString, QString>>
splitTopLevel(QStringView sv, QLatin1StringView delim)
{
    const qsizetype nDelim = delim.size();
    int parenDepth = 0;
    for (qsizetype i = 0, N = sv.size() - nDelim + 1; i < N; ++i)
    {
        const QChar c = sv[i];
        if      (c == u'(') ++parenDepth;
        else if (c == u')') --parenDepth;
        else if (parenDepth == 0 && sv.mid(i, nDelim) == delim)
        {
            return std::pair<QString, QString>{
                QString(sv.left(i)),
                QString(sv.mid(i + nDelim)) };
        }
    }
    return std::nullopt;
}

// Split at all occurrences of `delim` that are *not* inside parentheses.
// Returns vector of parts if any delimiters found; otherwise std::nullopt.
inline std::optional<QVector<QString>>
splitTopLevelMultiple(QStringView sv, QLatin1StringView delim)
{
    const qsizetype nDelim = delim.size();
    int parenDepth = 0;
    int bracketDepth = 0;  // Track square bracket depth
    QVector<QString> parts;
    qsizetype lastStart = 0;
    
    for (qsizetype i = 0, N = sv.size() - nDelim + 1; i < N; ++i)
    {
        const QChar c = sv[i];
        if      (c == u'(') ++parenDepth;
        else if (c == u')') --parenDepth;
        else if (c == u'[') ++bracketDepth;
        else if (c == u']') --bracketDepth;
        else if (parenDepth == 0 && bracketDepth == 0 && sv.mid(i, nDelim) == delim)
        {
            // Found delimiter at top level - add part
            parts.append(QString(sv.mid(lastStart, i - lastStart)));
            lastStart = i + nDelim;
        }
    }
    
    // Add final part after last delimiter
    if (!parts.isEmpty()) {
        parts.append(QString(sv.mid(lastStart)));
        return parts;
    }
    
    return std::nullopt;
}

} // namespace json_query::json_path::detail
