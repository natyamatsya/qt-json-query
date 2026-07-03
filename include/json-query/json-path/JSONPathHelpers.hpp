// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

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
#include <vector>
#include "json-query/json-path/JSONPathLog.hpp"

namespace json_query::json_path::detail
{

// Remove exactly one balanced pair of outer parentheses, then trim.
inline QString stripOuterParens(QStringView sv)
{
    if (sv.size() >= 2 && sv.front() == u'(' && sv.back() == u')')
        sv = sv.mid(1, sv.size() - 2);
    return QString(sv).trimmed();
}

// Split at the first occurrence of `delim` that is *not* inside parentheses.
// Returns {left, right} if found; otherwise std::nullopt.
inline std::optional<std::pair<QString, QString>> splitTopLevel(QStringView sv, QLatin1StringView delim)
{
    qCDebug(jsonPathLog) << "splitTopLevel: input=" << sv << "delim=" << delim;
    const auto nDelim{delim.size()};
    int        parenDepth{0};
    int        bracketDepth{0};
    int        braceDepth{0};
    bool       inQuote{false};
    QChar      quoteChar{};
    bool       escaped{false};

    const qsizetype N = sv.size();
    for (qsizetype i = 0; i < N; ++i)
    {
        const QChar c = sv[i];

        if (inQuote)
        {
            if (escaped)
            {
                escaped = false; // consume escaped char inside quotes
                continue;
            }
            if (c == u'\\')
            {
                escaped = true;
                continue;
            }
            if (c == quoteChar)
            {
                inQuote   = false;
                quoteChar = QChar{};
            }
            continue; // ignore structural chars inside quotes
        }

        // Not in quotes: track structural nesting
        if (c == u'\'')
        {
            inQuote   = true;
            quoteChar = u'\'';
            continue;
        }
        if (c == u'"')
        {
            inQuote   = true;
            quoteChar = u'"';
            continue;
        }
        if (c == u'(')
            ++parenDepth;
        else if (c == u')')
            --parenDepth;
        else if (c == u'[')
            ++bracketDepth;
        else if (c == u']')
            --bracketDepth;
        else if (c == u'{')
            ++braceDepth;
        else if (c == u'}')
            --braceDepth;

        // Only consider delimiter at top level and not inside quotes
        if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0)
        {
            if (i + nDelim <= N && sv.mid(i, nDelim) == delim)
            {
                auto left  = QString(sv.left(i));
                auto right = QString(sv.mid(i + nDelim));
                qCDebug(jsonPathLog) << "splitTopLevel: found split at position" << i << "left=" << left
                                     << "right=" << right;
                return std::pair<QString, QString>{left, right};
            }
        }
    }
    qCDebug(jsonPathLog) << "splitTopLevel: no split found for delim" << delim << "in" << sv;
    return std::nullopt;
}

// Split at all occurrences of `delim` that are *not* inside parentheses.
// Returns vector of parts if any delimiters found; otherwise std::nullopt.
inline std::optional<std::vector<QString>> splitTopLevelMultiple(QStringView sv, QLatin1StringView delim)
{
    const auto           nDelim{delim.size()};
    int                  parenDepth{0};
    int                  bracketDepth{0};
    int                  braceDepth{0};
    bool                 inQuote{false};
    QChar                quoteChar{};
    bool                 escaped{false};
    std::vector<QString> parts;
    qsizetype            lastStart{0};

    const qsizetype N = sv.size();
    for (qsizetype i = 0; i < N; ++i)
    {
        const QChar c = sv[i];

        if (inQuote)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (c == u'\\')
            {
                escaped = true;
                continue;
            }
            if (c == quoteChar)
            {
                inQuote   = false;
                quoteChar = QChar{};
            }
            continue;
        }

        if (c == u'\'')
        {
            inQuote   = true;
            quoteChar = u'\'';
            continue;
        }
        if (c == u'"')
        {
            inQuote   = true;
            quoteChar = u'"';
            continue;
        }
        if (c == u'(')
            ++parenDepth;
        else if (c == u')')
            --parenDepth;
        else if (c == u'[')
            ++bracketDepth;
        else if (c == u']')
            --bracketDepth;
        else if (c == u'{')
            ++braceDepth;
        else if (c == u'}')
            --braceDepth;

        if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0)
        {
            if (i + nDelim <= N && sv.mid(i, nDelim) == delim)
            {
                parts.push_back(QString(sv.mid(lastStart, i - lastStart)));
                lastStart = i + nDelim;
            }
        }
    }

    if (!parts.empty())
    {
        parts.push_back(QString(sv.mid(lastStart)));
        return parts;
    }

    return std::nullopt;
}

} // namespace json_query::json_path::detail
