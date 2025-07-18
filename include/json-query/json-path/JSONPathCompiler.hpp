#pragma once

// Initial scaffolding for the upcoming JSONPath parser split.
// In the next step we will move the full path-parsing logic from
// JSONPath.cpp into PathParser.cpp and expose it via the `parse` free
// function below.

#include <QVector>
#include <QString>
#include <QJsonValue>
#include <variant>
#include <tuple>
#include <functional>

namespace json_query::json_path
{
    enum class FunctionType { None, Length, Min, Max };

    // ======================================================================
    //  Compact, pre-decoded token layout
    // ======================================================================
    struct Slice { qsizetype start{}, end{}, step{}; };

    struct Token {
        enum class Kind : quint8 {
            Key, Index, Slice, Wildcard, Recursive, Filter
        };
        Kind          kind {Kind::Key};
        qsizetype     index{};          // for Kind::Index
        Slice         slice{};          // for Kind::Slice
        quint32       hash{};           // cached object-key hash
        QString       key{};            // for Kind::Key / Filter
        std::size_t   filterId{};       // index into filter table
    };   

// ------------------------------------------------------------------
//  Parser / compiler error codes
// ------------------------------------------------------------------
enum class Error : std::uint8_t {
    Ok = 0,               // not used in expected<T,E>
    MissingRoot,
    TrailingDot,
    EmptySegment,
    UnmatchedBracket,
    UnmatchedQuote,
    UnsupportedFilter,
    InvalidSlice,
    InvalidIndex
};

[[nodiscard]] inline constexpr std::string_view
toString(Error e) noexcept
{
    using enum Error;
    switch (e)
    {
        case MissingRoot      : return "path must start with '$' or '@'";
        case TrailingDot      : return "trailing '.'";
        case EmptySegment     : return "empty property segment";
        case UnmatchedBracket : return "unmatched '['";
        case UnmatchedQuote   : return "unmatched quote inside [...]";
        case UnsupportedFilter: return "unsupported filter expression";
        case InvalidSlice     : return "bad array slice";
        case InvalidIndex     : return "bad numeric index";
        default               : return "unknown error";
    }
}
} // namespace json_query::json_path
