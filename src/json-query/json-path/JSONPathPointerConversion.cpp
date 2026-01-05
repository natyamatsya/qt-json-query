// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathPointerConversion.hpp"

#include <QStringList>
#include <QtCore/QtCore>
#include <vector> // added include for std::vector
using namespace Qt::StringLiterals;

namespace json_query::json_path::detail
{

// escapePointerSegment is now inline in the header, delegating to json_pointer::escapeToken

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
