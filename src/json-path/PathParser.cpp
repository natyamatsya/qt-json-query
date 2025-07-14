#include "json-path/PathParser.hpp"

#include <QString>
#include <QPair>

namespace json_path
{
    ParseResult parse(const QString & /*originalPath*/)
    {
        // Stub implementation – will be replaced when actual parsing logic
        // is migrated from JSONPath.cpp in the next incremental slice.
        return {};
    }
} // namespace json_path

namespace json_path
{
    // Detect and remove a known trailing function suffix from the path string.
    // Returns the corresponding FunctionType and mutates the path in-place.
    FunctionType detectTrailingFunction(QString &path)
    {
        static const QPair<QString, FunctionType> funcs[] = {
            {QStringLiteral(".length()"), FunctionType::Length},
            {QStringLiteral(".min()"),    FunctionType::Min},
            {QStringLiteral(".max()"),    FunctionType::Max},
        };

        for (const auto &p : funcs)
        {
            if (path.endsWith(p.first))
            {
                path.chop(p.first.size()); // strip suffix
                return p.second;
            }
        }
        return FunctionType::None;
    }
    // Convert a simple JSONPath segment to an RFC-6901 JSON Pointer slice.
    QString segmentToPointer(const QString &segment)
    {
        if (segment.isEmpty() || segment == u"$")
            return {};

        QStringView sv{segment};
        qsizetype pos = 0;
        if (sv.at(0) == u'$')
            ++pos; // skip root symbol

        QString out;
        out.reserve(sv.size());

        auto flush = [&](qsizetype a, qsizetype b)
        {
            if (b > a)
            {
                out += u'/';
                out += sv.sliced(a, b - a);
            }
        };

        while (pos < sv.size())
        {
            const QChar c = sv.at(pos);
            if (c == u'.')
            {
                ++pos;
                continue;
            }
            if (c == u'[')
            {
                ++pos;
                if (pos >= sv.size()) break;

                // quoted property name
                if (sv.at(pos) == u'"' || sv.at(pos) == u'\'')
                {
                    const QChar quote = sv.at(pos);
                    ++pos;
                    qsizetype start = pos;
                    while (pos < sv.size() && sv.at(pos) != quote)
                        ++pos;
                    flush(start, pos);
                    while (pos < sv.size() && sv.at(pos) != u']')
                        ++pos;
                    ++pos; // skip ]
                    continue;
                }
                // numeric index or unquoted string till ]
                qsizetype start = pos;
                while (pos < sv.size() && sv.at(pos) != u']')
                    ++pos;
                flush(start, pos);
                ++pos;
                continue;
            }
            // plain until next . or [
            qsizetype start = pos;
            while (pos < sv.size() && sv.at(pos) != u'.' && sv.at(pos) != u'[')
                ++pos;
            flush(start, pos);
        }
        return out;
    }
}