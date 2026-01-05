// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathFilterParsers.hpp"
#include "json-query/json-path/JSONPathFilterComparisonDispatcher.hpp"
#include "json-query/json-path/JSONPathFilterComparison.hpp"
#include "json-query/json-path/JSONPathFilterHelpers.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <ctre.hpp>

namespace json_query::json_path::detail
{

using utils::to_qt_s;
using utils::to_sv;

// Existence filter type enumeration for compile-time dispatch
enum class ExistenceFilterType
{
    PropertyDot,     // @.prop
    PropertyBracket, // @["prop"]
    Root,            // @
    RootRef,         // $
    Wildcard,        // @.*
    ArraySlice,      // @[1:3]
    MultiSelector,   // @[0,'a',1]
    NestedFilter,    // @[?expr]
    // Negated variants
    NegPropertyDot,     // !@.prop
    NegPropertyBracket, // !@["prop"]
    NegRoot,            // !@
    NegRootRef,         // !$
    NegWildcard,        // !@.*
    NegArraySlice,      // !@[1:3]
    NegMultiSelector,   // !@[0,'a',1]
    NegNestedFilter,    // !@[?expr]
    Unknown
};

// Pattern descriptor for TableGen-like declarative dispatch
template <ExistenceFilterType Type>
struct ExistencePatternDef
{
    static constexpr const char* pattern = "";
    static constexpr bool        enabled = false;
};

// Template specializations for each existence filter type (TableGen-style definitions)

template <>
struct ExistencePatternDef<ExistenceFilterType::PropertyDot>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^@\.([\w$]+)$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::NegPropertyDot>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^!@\.([\w$]+)$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::PropertyBracket>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^@\[['\"]([^'"]+)['\"]\]$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::NegPropertyBracket>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^!@\[['\"]([^'"]+)['\"]\]$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::ArraySlice>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^@\[(-?\d+):(-?\d+)\]$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::NegArraySlice>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^!@\[(-?\d+):(-?\d+)\]$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::MultiSelector>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^@\[([^?:][^:]*)\]$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::NegMultiSelector>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^!@\[([^?:][^:]*)\]$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::NestedFilter>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^@\[\?(.+)\]$)"};
    static constexpr bool enabled = true;
};

template <>
struct ExistencePatternDef<ExistenceFilterType::NegNestedFilter>
{
    static constexpr auto pattern = ctll::fixed_string{R"(^!@\[\?(.+)\]$)"};
    static constexpr bool enabled = true;
};

// Token factory template specializations for each filter type
template <ExistenceFilterType Type>
struct ExistenceTokenFactory
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        return std::nullopt; // Default: not implemented
    }
};

// Property existence token factory
template <>
struct ExistenceTokenFactory<ExistenceFilterType::PropertyDot>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::PropertyDot>::enabled)
        {
            if (auto m = ctre::match<ExistencePatternDef<ExistenceFilterType::PropertyDot>::pattern>(to_sv(input)))
            {
                auto    prop = to_qt_s(m.template get<1>().to_view());
                Builder b{out};
                return b.add([prop](const QJsonValue& j) -> bool { return j.toObject().contains(prop); },
                             QString("@.%1").arg(prop));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::NegPropertyDot>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::NegPropertyDot>::enabled)
        {
            if (auto m = ctre::match<ExistencePatternDef<ExistenceFilterType::NegPropertyDot>::pattern>(to_sv(input)))
            {
                auto    prop = to_qt_s(m.template get<1>().to_view());
                Builder b{out};
                return b.add([prop](const QJsonValue& j) -> bool { return !j.toObject().contains(prop); },
                             QString("!@.%1").arg(prop));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::PropertyBracket>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::PropertyBracket>::enabled)
        {
            if (auto m = ctre::match<ExistencePatternDef<ExistenceFilterType::PropertyBracket>::pattern>(to_sv(input)))
            {
                auto    prop = to_qt_s(m.template get<1>().to_view());
                Builder b{out};
                return b.add([prop](const QJsonValue& j) -> bool { return j.toObject().contains(prop); },
                             QString("@[\"%1\"]").arg(prop));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::NegPropertyBracket>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::NegPropertyBracket>::enabled)
        {
            if (auto m =
                    ctre::match<ExistencePatternDef<ExistenceFilterType::NegPropertyBracket>::pattern>(to_sv(input)))
            {
                auto    prop = to_qt_s(m.template get<1>().to_view());
                Builder b{out};
                return b.add([prop](const QJsonValue& j) -> bool { return !j.toObject().contains(prop); },
                             QString("!@[\"%1\"]").arg(prop));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::ArraySlice>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::ArraySlice>::enabled)
        {
            if (auto m = ctre::match<ExistencePatternDef<ExistenceFilterType::ArraySlice>::pattern>(to_sv(input)))
            {
                auto    startStr = to_qt_s(m.template get<1>().to_view());
                auto    endStr   = to_qt_s(m.template get<2>().to_view());
                auto    start    = startStr.isEmpty() ? -1 : startStr.toInt();
                auto    end      = endStr.isEmpty() ? -1 : endStr.toInt();
                Builder b{out};
                return b.add(
                    [start, end](const QJsonValue& j) -> bool
                    {
                        const auto arr{j.toArray()};
                        auto       actualStart = (start == -1) ? 0 : start;
                        auto       actualEnd   = (end == -1) ? arr.size() : end;
                        return actualStart < arr.size() && actualEnd > actualStart;
                    },
                    QString("@[%1:%2]").arg(start).arg(end));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::NegArraySlice>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::NegArraySlice>::enabled)
        {
            if (auto m = ctre::match<ExistencePatternDef<ExistenceFilterType::NegArraySlice>::pattern>(to_sv(input)))
            {
                auto    startStr = to_qt_s(m.template get<1>().to_view());
                auto    endStr   = to_qt_s(m.template get<2>().to_view());
                auto    start    = startStr.isEmpty() ? -1 : startStr.toInt();
                auto    end      = endStr.isEmpty() ? -1 : endStr.toInt();
                Builder b{out};
                return b.add(
                    [start, end](const QJsonValue& j) -> bool
                    {
                        const auto arr{j.toArray()};
                        auto       actualStart = (start == -1) ? 0 : start;
                        auto       actualEnd   = (end == -1) ? arr.size() : end;
                        return actualStart >= arr.size() || actualEnd <= actualStart;
                    },
                    QString("!@[%1:%2]").arg(start).arg(end));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::MultiSelector>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::MultiSelector>::enabled)
        {
            if (auto m = ctre::match<ExistencePatternDef<ExistenceFilterType::MultiSelector>::pattern>(to_sv(input)))
            {
                auto    selectorsStr = to_qt_s(m.template get<1>().to_view());
                Builder b{out};
                return b.add(
                    [selectorsStr](const QJsonValue& j) -> bool
                    {
                        QStringList selectors = selectorsStr.split(',');
                        for (const QString& selectorRaw : selectors)
                        {
                            auto selector{selectorRaw.trimmed()};

                            if ((selector.startsWith('"') && selector.endsWith('"')) ||
                                (selector.startsWith('\'') && selector.endsWith('\'')))
                            {
                                auto key{selector.mid(1, selector.size() - 2)};
                                if (j.isObject() && j.toObject().contains(key))
                                    return true;
                            }
                            else
                            {
                                bool ok;
                                auto index{selector.toInt(&ok)};
                                if (ok && j.isArray())
                                {
                                    const auto arr{j.toArray()};
                                    if (index >= 0 && index < arr.size())
                                        return true;
                                }
                            }
                        }
                        return false;
                    },
                    QString("@[%1]").arg(selectorsStr));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::NegMultiSelector>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::NegMultiSelector>::enabled)
        {
            if (auto m =
                    ctre::match<ExistencePatternDef<ExistenceFilterType::NegMultiSelector>::pattern>(to_sv(input)))
            {
                auto    selectorsStr = to_qt_s(m.template get<1>().to_view());
                Builder b{out};
                return b.add(
                    [selectorsStr](const QJsonValue& j) -> bool
                    {
                        QStringList selectors = selectorsStr.split(',');
                        for (const QString& selectorRaw : selectors)
                        {
                            auto selector{selectorRaw.trimmed()};

                            if ((selector.startsWith('"') && selector.endsWith('"')) ||
                                (selector.startsWith('\'') && selector.endsWith('\'')))
                            {
                                auto key{selector.mid(1, selector.size() - 2)};
                                if (j.isObject() && j.toObject().contains(key))
                                    return false; // Found one, so negation is false
                            }
                            else
                            {
                                bool ok;
                                auto index{selector.toInt(&ok)};
                                if (ok && j.isArray())
                                {
                                    const auto arr{j.toArray()};
                                    if (index >= 0 && index < arr.size())
                                        return false; // Found one, so negation is false
                                }
                            }
                        }
                        return true; // None found, so negation is true
                    },
                    QString("!@[%1]").arg(selectorsStr));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::NestedFilter>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::NestedFilter>::enabled)
        {
            if (auto m = ctre::match<ExistencePatternDef<ExistenceFilterType::NestedFilter>::pattern>(to_sv(input)))
            {
                auto    filterExpr = to_qt_s(m.template get<1>().to_view());
                Builder b{out};
                return b.add(
                    [filterExpr](const QJsonValue& j) -> bool
                    {
                        if (!j.isArray())
                            return false;

                        const auto                                   arr{j.toArray()};
                        std::vector<json_query::json_path::FilterFn> innerFilterFns;
                        auto innerToken{json_query::json_path::compileFilter(filterExpr, innerFilterFns)};
                        if (!innerToken || innerFilterFns.empty())
                            return false;

                        json_query::json_path::FilterFn innerFilterFn = innerFilterFns.back();
                        for (const auto& element : arr)
                            if (innerFilterFn(element))
                                return true;
                        return false;
                    },
                    QString("@[?%1]").arg(filterExpr));
            }
        }
        return std::nullopt;
    }
};

template <>
struct ExistenceTokenFactory<ExistenceFilterType::NegNestedFilter>
{
    static std::optional<json_query::json_path::Token> create(const QString&                                input,
                                                              std::vector<json_query::json_path::FilterFn>& out)
    {
        if constexpr (ExistencePatternDef<ExistenceFilterType::NegNestedFilter>::enabled)
        {
            if (auto m = ctre::match<ExistencePatternDef<ExistenceFilterType::NegNestedFilter>::pattern>(to_sv(input)))
            {
                auto    filterExpr = to_qt_s(m.template get<1>().to_view());
                Builder b{out};
                return b.add(
                    [filterExpr](const QJsonValue& j) -> bool
                    {
                        if (!j.isArray())
                            return true; // Non-arrays don't match, so negation is true

                        const auto                                   arr{j.toArray()};
                        std::vector<json_query::json_path::FilterFn> innerFilterFns;
                        auto innerToken{json_query::json_path::compileFilter(filterExpr, innerFilterFns)};
                        if (!innerToken || innerFilterFns.empty())
                            return true;

                        json_query::json_path::FilterFn innerFilterFn = innerFilterFns.back();
                        for (const auto& element : arr)
                            if (innerFilterFn(element))
                                return false; // Found match, so negation is false
                        return true;          // No matches, so negation is true
                    },
                    QString("!@[?%1]").arg(filterExpr));
            }
        }
        return std::nullopt;
    }
};

// TableGen-inspired dispatch table with compile-time priority ordering
template <ExistenceFilterType... Types>
struct ExistenceDispatchTable
{
    static std::optional<json_query::json_path::Token> dispatch(const QString&                                input,
                                                                std::vector<json_query::json_path::FilterFn>& out)
    {
        std::optional<json_query::json_path::Token> result;
        ((result = ExistenceTokenFactory<Types>::create(input, out)) || ...);
        return result;
    }
};

// Dispatch with priority order: negated patterns first (more specific), then regular patterns
using ExistenceDispatcher = ExistenceDispatchTable<ExistenceFilterType::NegPropertyDot,
                                                   ExistenceFilterType::NegPropertyBracket,
                                                   ExistenceFilterType::NegArraySlice,
                                                   ExistenceFilterType::NegMultiSelector,
                                                   ExistenceFilterType::NegNestedFilter,
                                                   ExistenceFilterType::PropertyDot,
                                                   ExistenceFilterType::PropertyBracket,
                                                   ExistenceFilterType::ArraySlice,
                                                   ExistenceFilterType::MultiSelector,
                                                   ExistenceFilterType::NestedFilter>;

// Individual parser function implementations (non-template)
// Note: parseOr, parseAnd, parseIn, parseRegex are implemented in JSONPathFilterCore.cpp

std::optional<json_query::json_path::Token> parseExists(const QString&                                s,
                                                        std::vector<json_query::json_path::FilterFn>& out)
{
    // Execute TableGen-style dispatch
    if (auto result = ExistenceDispatcher::dispatch(s, out))
        return result;

    // Handle special cases that don't fit the pattern-based approach
    if (s == "@.*")
    {
        Builder b{out};
        return b.add(
            [](const QJsonValue& j) -> bool
            {
                if (j.isObject())
                    return !j.toObject().isEmpty();
                if (j.isArray())
                    return !j.toArray().isEmpty();
                return false;
            },
            QString("@.*"));
    }
    if (s == "!@.*")
    {
        Builder b{out};
        return b.add(
            [](const QJsonValue& j) -> bool
            {
                if (j.isObject())
                    return j.toObject().isEmpty();
                if (j.isArray())
                    return j.toArray().isEmpty();
                return true;
            },
            QString("!@.*"));
    }
    if (s == "@" || s == "$")
    {
        Builder b{out};
        return b.add([](const QJsonValue& j) -> bool { return !j.isUndefined(); }, s);
    }
    if (s == "!@" || s == "!$")
    {
        Builder b{out};
        return b.add([](const QJsonValue& j) -> bool { return j.isUndefined(); }, s);
    }

    return std::nullopt;
}

std::optional<json_query::json_path::Token> parseSelfCmp(const QString&                                s,
                                                         std::vector<json_query::json_path::FilterFn>& out)
{
    static constexpr auto pat = ctll::fixed_string{R"(^@\s*(==|!=|\>=|\<=|\>|\<)\s*@$)"};

    if (auto m = ctre::match<pat>(to_sv(s)))
    {
        const auto op = to_qt_s(m.template get<1>().to_view());

        Builder b{out};
        return b.add(
            [op](const QJsonValue& j) -> bool
            {
                // Self-comparison: compare the value to itself
                if (op == "==")
                    return true;
                if (op == "!=")
                    return false;
                // For ordering operators, self-comparison is always false
                return false;
            },
            QString("@%1@").arg(op));
    }

    return std::nullopt;
}

std::optional<json_query::json_path::Token> parseNot(const QString&                                s,
                                                     std::vector<json_query::json_path::FilterFn>& out)
{
    constexpr auto negParenPat = ctll::fixed_string{R"(^!\s*\(\s*(.*)\s*\)$)"};
    if (auto m = ctre::match<negParenPat>(to_sv(s)))
    {
        auto                                         innerExpr = to_qt_s(m.template get<1>().to_view()).trimmed();
        std::vector<json_query::json_path::FilterFn> innerFilters;
        if (auto innerToken = json_query::json_path::compileFilter(innerExpr, innerFilters))
        {
            Builder b{out};
            return b.add(
                [innerFilters, innerExpr](const QJsonValue& j) -> bool
                {
                    if (!innerFilters.empty())
                    {
                        auto innerResult{innerFilters[0](j)};
                        auto negatedResult{!innerResult};
                        return negatedResult;
                    }
                    return false;
                },
                QString("!(%1)").arg(innerExpr));
        }
    }
    if (s.startsWith('!') && s.length() > 1)
    {
        auto                                         innerExpr{s.mid(1).trimmed()};
        std::vector<json_query::json_path::FilterFn> innerFilters;
        if (auto innerToken = json_query::json_path::compileFilter(innerExpr, innerFilters))
        {
            Builder b{out};
            return b.add(
                [innerFilters, innerExpr](const QJsonValue& j) -> bool
                {
                    if (!innerFilters.empty())
                    {
                        auto innerResult{innerFilters[0](j)};
                        auto negatedResult{!innerResult};
                        return negatedResult;
                    }
                    return false;
                },
                QString("!%1").arg(innerExpr));
        }
    }
    return std::nullopt;
}

std::optional<json_query::json_path::Token> parseAbsolutePath(const QString&                                s,
                                                              std::vector<json_query::json_path::FilterFn>& out)
{
    // Check if expression starts with $ (absolute path reference)
    if (!s.startsWith('$'))
        return std::nullopt;

    // RFC 9535: Reject expressions that contain comparison operators
    // These should be handled by other filter rules, not absolute path parsing
    if (s.contains("==") || s.contains("!=") || s.contains("<=") || s.contains(">=") || s.contains("<") ||
        s.contains(">") || s.contains("&&") || s.contains("||"))
    {
        return std::nullopt;
    }

    // Only accept simple absolute path references like $, $.foo, $.*.a, etc.
    // Not complex expressions or comparisons

    // Try to create the JSONPath - if it fails, the pattern is invalid
    auto testPath{JSONPath::create(s)};
    if (!testPath)
        return std::nullopt; // Invalid absolute path pattern

    Builder b{out};
    return b.add(
        [s](const QJsonValue& rootValue) -> bool
        {
            // Create a temporary JSONPath to evaluate the absolute path
            // against the root document
            if (auto path = JSONPath::create(s))
            {
                auto results{path->evaluateAll(rootValue)};
                if (results)
                    return !results->isEmpty();
                return false;
            }
            return false;
        },
        s);
}

// Helper function to parse JSON literals from strings
QJsonValue parseJsonLiteral(const QString& value)
{
    // Refactored to use monadic error handling patterns
    const auto trimmed{value.trimmed()};

    // Parse literal using monadic pattern with optional chaining
    auto parseNull = [](const QString& s) -> std::optional<QJsonValue>
    { return (s == "null") ? std::optional<QJsonValue>{QJsonValue{}} : std::nullopt; };

    auto parseBoolean = [](const QString& s) -> std::optional<QJsonValue>
    {
        if (s == "true")
            return QJsonValue{true};
        if (s == "false")
            return QJsonValue{false};
        return std::nullopt;
    };

    auto parseNumber = [](const QString& s) -> std::optional<QJsonValue>
    {
        bool ok;
        auto num{s.toDouble(&ok)};
        return ok ? std::optional<QJsonValue>{QJsonValue{num}} : std::nullopt;
    };

    auto parseQuotedString = [](const QString& s) -> std::optional<QJsonValue>
    {
        if ((s.startsWith('"') && s.endsWith('"')) || (s.startsWith('\'') && s.endsWith('\'')))
            return QJsonValue{s.mid(1, s.length() - 2)};
        return std::nullopt;
    };

    // Try parsing in order using monadic chaining
    return parseNull(trimmed)
        .or_else([&]() { return parseBoolean(trimmed); })
        .or_else([&]() { return parseNumber(trimmed); })
        .or_else([&]() { return parseQuotedString(trimmed); })
        .value_or(QJsonValue{trimmed}); // Default to string
}

std::optional<Token> parseRegex(const QString& s, std::vector<FilterFn>& out)
{
    constexpr auto dotPat = ctll::fixed_string{R"(@\.([\w$]+)\s*=~\s*/(.+)/)"};
    constexpr auto brkPat = ctll::fixed_string{R"(@\[['\"]([^'"]+)['\"]\]\s*=~\s*/(.+)/)"};

    if (auto t = parseRegex1<dotPat>(s, out))
        return t;
    return parseRegex1<brkPat>(s, out);
}

std::optional<Token> parseCompare(const QString& s, std::vector<FilterFn>& out)
{
    // Use TableGen-inspired compile-time dispatch with priority ordering
    return ComparisonDispatcher::dispatch(s, out);
}

} // namespace json_query::json_path::detail
