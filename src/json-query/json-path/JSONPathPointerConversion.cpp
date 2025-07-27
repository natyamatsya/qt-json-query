#include "json-query/json-path/JSONPathPointerConversion.hpp"

#include <QStringList>
#include <QtCore/QtCore>
#include <vector> // added include for std::vector
using namespace Qt::StringLiterals;

namespace json_query::json_path::detail
{

QString escapePointerSegment(const QString& seg)
{
    QString out;
    out.reserve(seg.size());
    for (QChar c : seg)
        if (c == u'~')
            out += QStringLiteral("~0");
        else if (c == u'/')
            out += QStringLiteral("~1");
        else
            out += c;
    return out;
}

QString tokensToPointer(QStringList& segments, const std::vector<Token>& tokens)
{
    // assume tokens[0] is root ($)
    for (qsizetype i = 1; i < tokens.size(); ++i)
    {
        const auto& tk = tokens[i];
        switch (tk.kind)
        {
        case Token::Kind::Key:
            segments.append(escapePointerSegment(tk.key));
            break;
        case Token::Kind::Index:
            segments.append(QString::number(tk.index));
            break;
        default:
            return QString{}; // unsupported for pointer mode
        }
    }
    return QStringLiteral("/") + segments.join(u'/');
}

} // namespace json_query::json_path::detail
