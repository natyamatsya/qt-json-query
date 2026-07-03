// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-path/JSONPathFilterComparison.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

using utils::to_qt_s;
using utils::to_sv;

// ============================================================================
// Pattern Definition Template Specializations
// ============================================================================

// Null comparison patterns
template <>
struct ComparisonPatternDef<ComparisonFilterType::NullPropertyDot>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\.([\w$]+)\s*(==|!=)\s*null)"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::NullPropertyBracket>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\[['\"]([^'"]+)['\"]\]\s*(==|!=)\s*null)"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::NullArrayIndex>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\[(-?\d+)\]\s*(==|!=)\s*null)"};
};

// Self comparison patterns
template <>
struct ComparisonPatternDef<ComparisonFilterType::DirectSelf>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\s*(==|!=)\s*@)"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::SelfPropertyDot>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\.([\w$]+)\s*(==|!=|<|>|<=|>=)\s*@)"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::SelfPropertyBracket>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\[['\"]([^'"]+)['\"]\]\s*(==|!=|<|>|<=|>=)\s*@)"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::SelfArrayIndex>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\[(-?\d+)\]\s*(==|!=|<|>|<=|>=)\s*@)"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::SelfValue>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\s*(==|!=|<|>|<=|>=)\s*(.+))"};
};

// Property-to-property comparison patterns
template <>
struct ComparisonPatternDef<ComparisonFilterType::PropertyToProperty>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\.([\w$]+)\s*(==|!=|<|>|<=|>=)\s*@\.([\w$]+))"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::PropertyToArray>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\.([\w$]+)\s*(==|!=|<|>|<=|>=)\s*@\.([\w$]+)\[(-?\d+)\])"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::ArrayToProperty>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\.([\w$]+)\[(-?\d+)\]\s*(==|!=|<|>|<=|>=)\s*@\.([\w$]+))"};
};

// Basic property comparison patterns
template <>
struct ComparisonPatternDef<ComparisonFilterType::BasicPropertyDot>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*(.+))"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::BasicPropertyBracket>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\[['\"]([^'"]+)['\"]\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
};

template <>
struct ComparisonPatternDef<ComparisonFilterType::BasicArrayIndex>
{
    static constexpr bool               enabled = true;
    static constexpr ctll::fixed_string pattern{R"(@\[(-?\d+)\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
};

// ============================================================================
// Token Factory Template Specializations
// ============================================================================

// Null comparison factories
template <>
struct ComparisonTokenFactory<ComparisonFilterType::NullPropertyDot>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::NullPropertyDot>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};

            Builder b{out};
            return b.add(
                [prop = std::move(prop), op = std::move(op)](const QJsonValue& j)
                {
                    const auto obj{j.toObject()};
                    const auto v{obj.value(prop)};
                    return performComparison(v, op, QJsonValue{QJsonValue::Null});
                },
                QString("@.%1").arg(prop));
        }
        return std::nullopt;
    }
};

template <>
struct ComparisonTokenFactory<ComparisonFilterType::NullPropertyBracket>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::NullPropertyBracket>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};

            Builder b{out};
            return b.add(
                [prop = std::move(prop), op = std::move(op)](const QJsonValue& j)
                {
                    const auto obj{j.toObject()};
                    const auto v{obj.value(prop)};
                    return performComparison(v, op, QJsonValue{QJsonValue::Null});
                },
                QString("@[\"%1\"]").arg(prop));
        }
        return std::nullopt;
    }
};

template <>
struct ComparisonTokenFactory<ComparisonFilterType::NullArrayIndex>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::NullArrayIndex>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};

            Builder b{out};
            return b.add(
                [prop = std::move(prop), op = std::move(op)](const QJsonValue& j)
                {
                    bool ok;
                    auto index{prop.toInt(&ok)};
                    if (!ok)
                        return false; // Invalid index

                    if (j.isArray())
                    {
                        const auto arr{asArray(j)};
                        if (index < 0 || index >= arr.size())
                        {
                            // Out of bounds: compare with undefined/null
                            QJsonValue undefined; // QJsonValue::Undefined
                            return performComparison(undefined, op, QJsonValue{QJsonValue::Null});
                        }
                        else
                        {
                            const auto v{arr[index]};
                            return performComparison(v, op, QJsonValue{QJsonValue::Null});
                        }
                    }
                    else
                    {
                        // Non-arrays don't have array indices: compare with undefined/null
                        QJsonValue undefined; // QJsonValue::Undefined
                        return performComparison(undefined, op, QJsonValue{QJsonValue::Null});
                    }
                },
                QString("@[%1]").arg(prop));
        }
        return std::nullopt;
    }
};

// Self comparison factories
template <>
struct ComparisonTokenFactory<ComparisonFilterType::DirectSelf>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::DirectSelf>::pattern>(to_sv(s)))
        {
            auto&& op{to_qt_s(m.template get<1>().to_view())};

            Builder b{out};
            return b.add([op = std::move(op)](const QJsonValue& j) { return performComparison(j, op, j); },
                         QString("@%1@").arg(op));
        }
        return std::nullopt;
    }
};

template <>
struct ComparisonTokenFactory<ComparisonFilterType::SelfPropertyDot>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::SelfPropertyDot>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};

            Builder b{out};
            return b.add(
                [prop = std::move(prop), op = std::move(op)](const QJsonValue& j)
                {
                    const auto obj{j.toObject()};
                    const auto v{obj.value(prop)};
                    return performComparison(v, op, j);
                },
                QString("@.%1%2@").arg(prop, op));
        }
        return std::nullopt;
    }
};

template <>
struct ComparisonTokenFactory<ComparisonFilterType::SelfPropertyBracket>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::SelfPropertyBracket>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};

            Builder b{out};
            return b.add(
                [prop = std::move(prop), op = std::move(op)](const QJsonValue& j)
                {
                    const auto obj{j.toObject()};
                    const auto v{obj.value(prop)};
                    return performComparison(v, op, j);
                },
                QString("@[\"%1\"]%2@").arg(prop, op));
        }
        return std::nullopt;
    }
};

template <>
struct ComparisonTokenFactory<ComparisonFilterType::SelfArrayIndex>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::SelfArrayIndex>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};

            Builder b{out};
            return b.add(
                [prop = std::move(prop), op = std::move(op)](const QJsonValue& j)
                {
                    bool ok;
                    auto index{prop.toInt(&ok)};
                    if (!ok)
                        return false; // Invalid index

                    if (j.isArray())
                    {
                        const auto arr{asArray(j)};
                        if (index < 0 || index >= arr.size())
                        {
                            // Out of bounds: compare with undefined
                            QJsonValue undefined; // QJsonValue::Undefined
                            return performComparison(undefined, op, j);
                        }
                        else
                        {
                            const auto v{arr[index]};
                            return performComparison(v, op, j);
                        }
                    }
                    else
                    {
                        // Non-arrays don't have array indices: compare with undefined
                        QJsonValue undefined; // QJsonValue::Undefined
                        return performComparison(undefined, op, j);
                    }
                },
                QString("@[%1]%2@").arg(prop, op));
        }
        return std::nullopt;
    }
};

template <>
struct ComparisonTokenFactory<ComparisonFilterType::SelfValue>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::SelfValue>::pattern>(to_sv(s)))
        {
            auto&& op{to_qt_s(m.template get<1>().to_view())};
            auto&& rhs{to_qt_s(m.template get<2>().to_view())};

            // Parse RHS value using existing comparison context logic
            auto ctx{parseRhsValue(op, rhs)};
            if (!ctx)
                return std::nullopt;

            Builder b{out};
            return b.add([ctx = *ctx](const QJsonValue& j) { return ctx.compare(j); }, s);
        }
        return std::nullopt;
    }
};

// Basic property comparison factories
template <>
struct ComparisonTokenFactory<ComparisonFilterType::BasicPropertyDot>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::BasicPropertyDot>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};
            auto&& rhs{to_qt_s(m.template get<3>().to_view())};

            // Parse RHS value using existing comparison context logic
            auto ctx{parseRhsValue(op, rhs)};
            if (!ctx)
                return std::nullopt;

            Builder b{out};
            return b.add(
                [prop = std::move(prop), ctx = *ctx](const QJsonValue& j)
                {
                    const auto obj{j.toObject()};
                    const auto v{obj.value(prop)};
                    return ctx.compare(v);
                },
                prop);
        }
        return std::nullopt;
    }
};

template <>
struct ComparisonTokenFactory<ComparisonFilterType::BasicPropertyBracket>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::BasicPropertyBracket>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};
            auto&& rhs{to_qt_s(m.template get<3>().to_view())};

            // Parse RHS value using existing comparison context logic
            auto ctx{parseRhsValue(op, rhs)};
            if (!ctx)
                return std::nullopt;

            Builder b{out};
            return b.add(
                [prop = std::move(prop), ctx = *ctx](const QJsonValue& j)
                {
                    const auto obj{j.toObject()};
                    const auto v{obj.value(prop)};
                    return ctx.compare(v);
                },
                QString("@[\"%1\"]").arg(prop));
        }
        return std::nullopt;
    }
};

template <>
struct ComparisonTokenFactory<ComparisonFilterType::BasicArrayIndex>
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        if (auto m = ctre::match<ComparisonPatternDef<ComparisonFilterType::BasicArrayIndex>::pattern>(to_sv(s)))
        {
            auto&& prop{to_qt_s(m.template get<1>().to_view())};
            auto&& op{to_qt_s(m.template get<2>().to_view())};
            auto&& rhs{to_qt_s(m.template get<3>().to_view())};

            // Parse RHS value using existing comparison context logic
            auto ctx{parseRhsValue(op, rhs)};
            if (!ctx)
                return std::nullopt;

            Builder b{out};
            return b.add(
                [prop = std::move(prop), ctx = *ctx](const QJsonValue& j)
                {
                    bool ok;
                    auto index{prop.toInt(&ok)};
                    if (!ok)
                        return false; // Invalid index

                    if (j.isArray())
                    {
                        const auto arr{asArray(j)};
                        if (index < 0 || index >= arr.size())
                        {
                            // Out of bounds: compare with undefined
                            QJsonValue undefined; // QJsonValue::Undefined
                            return ctx.compare(undefined);
                        }
                        else
                        {
                            const auto v{arr[index]};
                            return ctx.compare(v);
                        }
                    }
                    else
                    {
                        // Non-arrays don't have array indices: compare with undefined
                        QJsonValue undefined; // QJsonValue::Undefined
                        return ctx.compare(undefined);
                    }
                },
                QString("@[%1]").arg(prop));
        }
        return std::nullopt;
    }
};

// ============================================================================
// Dispatch Table Implementation
// ============================================================================

template <ComparisonFilterType... Types>
std::optional<Token> ComparisonDispatchTable<Types...>::dispatch(const QString& s, std::vector<FilterFn>& out)
{
    return dispatchImpl<Types...>(s, out);
}

template <ComparisonFilterType... Types>
template <ComparisonFilterType First, ComparisonFilterType... Rest>
std::optional<Token> ComparisonDispatchTable<Types...>::dispatchImpl(const QString& s, std::vector<FilterFn>& out)
{
    if (auto result = ComparisonTokenFactory<First>::create(s, out))
        return result;
    if constexpr (sizeof...(Rest) > 0)
        return dispatchImpl<Rest...>(s, out);
    return std::nullopt;
}

} // namespace json_query::json_path::detail

// ============================================================================
// Explicit Template Instantiation
// ============================================================================

// Explicitly instantiate the dispatch table template for all comparison filter types
template class json_query::json_path::detail::ComparisonDispatchTable<
    json_query::json_path::detail::ComparisonFilterType::NullPropertyDot,
    json_query::json_path::detail::ComparisonFilterType::NullPropertyBracket,
    json_query::json_path::detail::ComparisonFilterType::NullArrayIndex,
    json_query::json_path::detail::ComparisonFilterType::DirectSelf,
    json_query::json_path::detail::ComparisonFilterType::SelfPropertyDot,
    json_query::json_path::detail::ComparisonFilterType::SelfPropertyBracket,
    json_query::json_path::detail::ComparisonFilterType::SelfArrayIndex,
    json_query::json_path::detail::ComparisonFilterType::SelfValue,
    json_query::json_path::detail::ComparisonFilterType::PropertyToProperty,
    json_query::json_path::detail::ComparisonFilterType::PropertyToArray,
    json_query::json_path::detail::ComparisonFilterType::ArrayToProperty,
    json_query::json_path::detail::ComparisonFilterType::BasicPropertyDot,
    json_query::json_path::detail::ComparisonFilterType::BasicPropertyBracket,
    json_query::json_path::detail::ComparisonFilterType::BasicArrayIndex>;
