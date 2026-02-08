// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathFilterParsers.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/json-path/JSONPathFilterComparison.hpp"
#include "json-query/json-path/JSONPathFilterExistenceParsers.hpp"
#include "json-query/json-path/JSONPathFilterFunctions.hpp"
#include "json-query/json-path/JSONPathFilterHelpers.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/utils/QtStringLiterals.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include <QJsonDocument>
#include <ctre.hpp>

namespace json_query::json_path::detail
{

using json_query::json_path::FilterFn;
using json_query::json_path::Token;
using json_query::json_path::detail::parseJsonLiteral;
using json_query::json_path::detail::parseRegex;
using json_query::json_path::detail::parseRegex1;
using json_query::json_path::detail::performComparison;
using json_query::json_path::detail::splitTopLevel;
using json_query::json_path::detail::stripOuterParens;
using json_query::utils::to_qt_s;
using json_query::utils::to_sv;
using json_query::literals::operator""_qt_l1;

// ============================================================================
// Basic Parser Function Implementations
// ============================================================================

std::optional<Token> parseOr(const QString& s, std::vector<FilterFn>& out)
{
    if (auto split = splitTopLevel(s, "||"_qt_l1); split)
    {
        const auto& [lhsS, rhsS] = *split;

        std::vector<FilterFn> tmp;
        auto                  lhsT{json_query::json_path::compileFilter(lhsS.trimmed(), tmp)};
        auto                  rhsT{json_query::json_path::compileFilter(rhsS.trimmed(), tmp)};
        if (!lhsT || !rhsT || tmp.empty())
            return std::nullopt;

        Builder  b{out};
        FilterFn lhs = *(tmp.end() - 2);
        FilterFn rhs = *(tmp.end() - 1);
        return b.add([lhs, rhs](const QJsonValue& j) { return lhs(j) || rhs(j); });
    }
    return std::nullopt;
}

std::optional<Token> parseAnd(const QString& s, std::vector<FilterFn>& out)
{
    if (auto split = splitTopLevel(s, "&&"_qt_l1); split)
    {
        const auto& [lhsS, rhsS] = *split;

        std::vector<FilterFn> tmp;
        auto                  lhsT{json_query::json_path::compileFilter(lhsS.trimmed(), tmp)};
        auto                  rhsT{json_query::json_path::compileFilter(rhsS.trimmed(), tmp)};
        if (!lhsT || !rhsT || tmp.empty())
            return std::nullopt;

        Builder  b{out};
        FilterFn lhs = *(tmp.end() - 2);
        FilterFn rhs = *(tmp.end() - 1);
        return b.add([lhs, rhs](const QJsonValue& j) { return lhs(j) && rhs(j); });
    }
    return std::nullopt;
}

std::optional<Token> parseIn(const QString& s, std::vector<FilterFn>& out)
{
    constexpr auto pat = ctll::fixed_string{R"('\s*([^']+?)\s*'\s+in\s+@\[['\"]([^'"]+)['\"]\])"};
    if (auto m = ctre::match<pat>(to_sv(s)))
    {
        auto&& want{to_qt_s(m.template get<1>().to_view())};
        auto&& array{to_qt_s(m.template get<2>().to_view())};

        Builder b{out};
        return b.add(
            [want = std::move(want), array = std::move(array)](const QJsonValue& j)
            {
                auto a{j[array]};
                if (!a.isArray())
                    return false;

                // Use ContainerCursor for optimized, zero-copy array iteration during 'in' evaluation
                const auto arr{asArray(a)};
                auto       cursor{internal::ContainerCursor::array(arr)};
                for (const auto& v : cursor)
                    if (v.isString() && v.toString() == want)
                        return true;
                return false;
            },
            array);
    }
    return std::nullopt;
}

// ============================================================================
// Modern Embedded Filter Parser Implementations (Zero-Overhead)
// ============================================================================

std::optional<Token> parseEmbeddedOr(const QString& s)
{
    // Simple debug output to verify execution
    qCDebug(jsonPathLog) << "DEBUG: parseEmbeddedOr called with input:" << s;
    qCDebug(jsonPathLog) << "parseEmbeddedOr: input=" << s;
    if (auto split = splitTopLevel(s, "||"_qt_l1); split)
    {
        auto [lhs, rhs] = *split;
        qCDebug(jsonPathLog) << "parseEmbeddedOr: split found - lhs=" << lhs << "rhs=" << rhs;

        auto leftToken{compileEmbeddedFilter(lhs.trimmed())};
        auto rightToken{compileEmbeddedFilter(rhs.trimmed())};

        if (!leftToken || !rightToken)
        {
            qCDebug(jsonPathLog) << "parseEmbeddedOr: failed to compile sub-tokens, leftToken="
                                 << (leftToken ? "valid" : "null") << "rightToken=" << (rightToken ? "valid" : "null");
            return std::nullopt;
        }

        // Create composite OR filter using embedded filter composition
        Token result;
        result.kind = Token::Kind::Filter;
        result.key  = QString("(%1)||(%2)").arg(lhs, rhs);
        qCDebug(jsonPathLog) << "parseEmbeddedOr: created composite OR token with key=" << result.key;

        // Embed a composite filter that evaluates both sides with OR logic
        result.embedFilter(
            [leftToken = *leftToken, rightToken = *rightToken](const QJsonValue& value) -> bool
            {
                auto leftResult{leftToken.evaluateEmbeddedFilter(value)};
                auto rightResult{rightToken.evaluateEmbeddedFilter(value)};
                qCDebug(jsonPathLog) << "parseEmbeddedOr: evaluating OR - leftResult=" << leftResult
                                     << "rightResult=" << rightResult << "final=" << (leftResult || rightResult);
                return leftResult || rightResult;
            });

        return result;
    }
    qCDebug(jsonPathLog) << "parseEmbeddedOr: no || split found in" << s;
    return std::nullopt;
}

std::optional<Token> parseEmbeddedAnd(const QString& s)
{
    qCDebug(jsonPathLog) << "parseEmbeddedAnd: input=" << s;
    if (auto split = splitTopLevel(s, "&&"_qt_l1); split)
    {
        auto [lhs, rhs] = *split;
        qCDebug(jsonPathLog) << "parseEmbeddedAnd: split found - lhs=" << lhs << "rhs=" << rhs;

        auto leftToken{compileEmbeddedFilter(lhs.trimmed())};
        auto rightToken{compileEmbeddedFilter(rhs.trimmed())};

        if (!leftToken || !rightToken)
        {
            qCDebug(jsonPathLog) << "parseEmbeddedAnd: failed to compile sub-tokens, leftToken="
                                 << (leftToken ? "valid" : "null") << "rightToken=" << (rightToken ? "valid" : "null");
            return std::nullopt;
        }

        // Create composite AND filter using embedded filter composition
        Token result;
        result.kind = Token::Kind::Filter;
        result.key  = QString("(%1)&&(%2)").arg(lhs, rhs);
        qCDebug(jsonPathLog) << "parseEmbeddedAnd: created composite AND token with key=" << result.key;

        // Embed a composite filter that evaluates both sides with AND logic
        result.embedFilter(
            [leftToken = *leftToken, rightToken = *rightToken](const QJsonValue& value) -> bool
            {
                auto leftResult{leftToken.evaluateEmbeddedFilter(value)};
                auto rightResult{rightToken.evaluateEmbeddedFilter(value)};
                qCDebug(jsonPathLog) << "parseEmbeddedAnd: evaluating AND - leftResult=" << leftResult
                                     << "rightResult=" << rightResult << "final=" << (leftResult && rightResult);
                return leftResult && rightResult;
            });

        return result;
    }
    qCDebug(jsonPathLog) << "parseEmbeddedAnd: no && split found in" << s;
    return std::nullopt;
}

std::optional<Token> parseEmbeddedIn(const QString& s)
{
    // Simplified implementation for now - can be enhanced later
    return std::nullopt;
}

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedComparePropToProp(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s)))
    {
        auto&& leftProp{to_qt_s(m.template get<1>().to_view())};
        auto&& op{to_qt_s(m.template get<2>().to_view())};
        auto&& rightProp{to_qt_s(m.template get<3>().to_view())};

        Token token;
        token.kind = Token::Kind::Filter;
        token.key  = s;

        token.embedFilter(
            [leftProp = std::move(leftProp), op = std::move(op), rightProp = std::move(rightProp)](const QJsonValue& j)
            {
                // Property-to-property comparison: @.a==@.b
                if (!j.isObject())
                    return false;

                const auto obj{j.toObject()};
                const auto leftValue{obj.value(leftProp)};
                const auto rightValue{obj.value(rightProp)};

                // Use the same logic as legacy performComparison function
                // Handle undefined values
                if (leftValue.type() == QJsonValue::Undefined || rightValue.type() == QJsonValue::Undefined)
                {
                    if (op == "==")
                        return leftValue.type() == rightValue.type(); // both undefined
                    if (op == "!=")
                        return leftValue.type() != rightValue.type(); // one undefined, one not
                    return false;                                     // ordering comparisons require same type
                }

                // Use deep equality for == and !=
                if (op == "==")
                    return leftValue == rightValue;
                if (op == "!=")
                    return leftValue != rightValue;

                // For ordering comparisons, ensure same type
                if (leftValue.type() != rightValue.type())
                    return false;

                // Handle ordering comparisons by type
                if (leftValue.isDouble() && rightValue.isDouble())
                {
                    auto left{leftValue.toDouble()};
                    auto right{rightValue.toDouble()};
                    if (op == "<")
                        return left < right;
                    if (op == ">")
                        return left > right;
                    if (op == "<=")
                        return left <= right;
                    if (op == ">=")
                        return left >= right;
                }
                else if (leftValue.isBool() && rightValue.isBool())
                {
                    auto left{leftValue.toBool()};
                    auto right{rightValue.toBool()};
                    if (op == "<")
                        return !left && right; // false < true
                    if (op == ">")
                        return left && !right; // true > false
                    if (op == "<=")
                        return !left || right; // false <= anything, true <= true
                    if (op == ">=")
                        return left || !right; // true >= anything, false >= false
                }
                else if (leftValue.isString() && rightValue.isString())
                {
                    auto left{leftValue.toString()};
                    auto right{rightValue.toString()};
                    if (op == "<")
                        return left < right;
                    if (op == ">")
                        return left > right;
                    if (op == "<=")
                        return left <= right;
                    if (op == ">=")
                        return left >= right;
                }

                return false; // unsupported comparison
            });

        return token;
    }
    return std::nullopt;
}

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedComparePropToArrayIdx(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s)))
    {
        auto&& leftProp{to_qt_s(m.template get<1>().to_view())};
        auto&& op{to_qt_s(m.template get<2>().to_view())};
        auto&& rightProp{to_qt_s(m.template get<3>().to_view())};
        auto&& rightIndex{to_qt_s(m.template get<4>().to_view())};

        Token token;
        token.kind = Token::Kind::Filter;
        token.key  = s;

        token.embedFilter(
            [leftProp   = std::move(leftProp),
             op         = std::move(op),
             rightProp  = std::move(rightProp),
             rightIndex = std::move(rightIndex)](const QJsonValue& j)
            {
                // Property-to-array-index comparison: @.a==@.list[9]
                if (!j.isObject())
                    return false;

                const auto obj{j.toObject()};
                const auto leftValue{obj.value(leftProp)};
                const auto rightArr{asArray(obj.value(rightProp))};

                bool       ok;
                auto       idx{rightIndex.toInt(&ok)};
                QJsonValue rightValue;

                // Handle out-of-bounds or invalid index as undefined
                if (!ok || idx < 0 || idx >= rightArr.size())
                    rightValue = QJsonValue{QJsonValue::Undefined};
                else
                    rightValue = rightArr.at(idx);

                // Use the same logic as legacy performComparison function
                // Handle undefined values
                if (leftValue.type() == QJsonValue::Undefined || rightValue.type() == QJsonValue::Undefined)
                {
                    if (op == "==")
                        return leftValue.type() == rightValue.type(); // both undefined
                    if (op == "!=")
                        return leftValue.type() != rightValue.type(); // one undefined, one not
                    return false;                                     // ordering comparisons require same type
                }

                // Use deep equality for == and !=
                if (op == "==")
                    return leftValue == rightValue;
                if (op == "!=")
                    return leftValue != rightValue;

                // For ordering comparisons, ensure same type
                if (leftValue.type() != rightValue.type())
                    return false;

                // Handle ordering comparisons by type
                if (leftValue.isDouble() && rightValue.isDouble())
                {
                    auto left{leftValue.toDouble()};
                    auto right{rightValue.toDouble()};
                    if (op == "<")
                        return left < right;
                    if (op == ">")
                        return left > right;
                    if (op == "<=")
                        return left <= right;
                    if (op == ">=")
                        return left >= right;
                }
                else if (leftValue.isBool() && rightValue.isBool())
                {
                    auto left{leftValue.toBool()};
                    auto right{rightValue.toBool()};
                    if (op == "<")
                        return !left && right; // false < true
                    if (op == ">")
                        return left && !right; // true > false
                    if (op == "<=")
                        return !left || right; // false <= anything, true <= true
                    if (op == ">=")
                        return left || !right; // true >= anything, false >= false
                }
                else if (leftValue.isString() && rightValue.isString())
                {
                    auto left{leftValue.toString()};
                    auto right{rightValue.toString()};
                    if (op == "<")
                        return left < right;
                    if (op == ">")
                        return left > right;
                    if (op == "<=")
                        return left <= right;
                    if (op == ">=")
                        return left >= right;
                }

                return false; // unsupported comparison
            });

        return token;
    }
    return std::nullopt;
}

std::optional<Token> parseEmbeddedCompare(const QString& s)
{
    QString localS{s}; // Create local copy for modification
    localS = detail::stripOuterParens(localS);

    // Trim whitespace from logical operator splitting
    localS = localS.trimmed();

    // Try embedded comparison patterns using the template functions
    constexpr auto dotPat  = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    constexpr auto brkPat  = ctll::fixed_string{R"(@\[['\"]([^'"]+)['\"]\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    constexpr auto idxPat  = ctll::fixed_string{R"(@\[(-?\d+)\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    constexpr auto selfPat = ctll::fixed_string{R"(^@\s*(==|!=|>=|<=|>|<)\s*(.+)$)"};
    constexpr auto selfSelfPat =
        ctll::fixed_string{R"(^(@|\$)\s*(==|!=|>=|<=|>|<)\s*(@|\$)$)"}; // Self-comparison: @==@, $==$, etc.
    constexpr auto propToPropPat =
        ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*@\.([\w$]+))"}; // Property-to-property: @.a==@.b
    constexpr auto propToArrayIdxPat = ctll::fixed_string{
        R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*@\.([\w$]+)\[(-?\d+)\])"};    // Property-to-array-index: @.a==@.list[9]
    constexpr auto nestedFilterPat = ctll::fixed_string{R"(@\[\?(.+)\])"}; // Nested filter: @[?@>1]

    // Try self-comparison pattern first (more specific)
    if (auto m = ctre::match<selfSelfPat>(to_sv(localS)))
    {
        auto&& leftSide{to_qt_s(m.template get<1>().to_view())};
        auto&& op{to_qt_s(m.template get<2>().to_view())};
        auto&& rightSide{to_qt_s(m.template get<3>().to_view())};

        // Only handle true self-comparison where both sides are the same
        if (leftSide == rightSide)
        {
            Token token;
            token.kind = Token::Kind::Filter;
            token.key  = localS;

            token.embedFilter(
                [op = std::move(op)](const QJsonValue& j)
                {
                    // Self-comparison: compare the value with itself
                    if (op == "==")
                        return true; // value always equals itself
                    if (op == "!=")
                        return false; // value never not-equals itself
                    if (op == ">=")
                        return true; // value always >= itself
                    if (op == "<=")
                        return true; // value always <= itself
                    if (op == ">")
                        return false; // value never > itself
                    if (op == "<")
                        return false; // value never < itself
                    return false;     // Unknown operator
                });

            return token;
        }
    }

    if (auto t = parseEmbeddedCompare1<dotPat>(localS))
        return t;
    if (auto t = parseEmbeddedCompare1<brkPat>(localS))
        return t;
    if (auto t = parseEmbeddedCompareIndex<idxPat>(localS))
        return t;
    if (auto t = parseEmbeddedComparePropToProp<propToPropPat>(localS))
        return t;
    if (auto t = parseEmbeddedComparePropToArrayIdx<propToArrayIdxPat>(localS))
        return t;
    if (auto t = parseEmbeddedSelfValue<selfPat>(localS))
        return t;

    return std::nullopt;
}

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedComparePropToArrayIdx(const QString& s);

std::optional<Token> parseEmbeddedRegex(const QString& s) { return std::nullopt; }

std::optional<Token> parseEmbeddedSelfCmp(const QString& s)
{
    constexpr auto selfPat = ctll::fixed_string{R"(^@\s*(==|!=|>=|<=|>|<)\s*(.+)$)"};
    return parseEmbeddedSelfValue<selfPat>(s);
}

std::optional<Token> parseEmbeddedNot(const QString& s)
{
    // Check if the expression starts with '!' (negation)
    if (s.startsWith('!'))
    {
        auto innerExpr{s.mid(1).trimmed()};

        // Parse the inner expression recursively
        auto innerToken{compileEmbeddedFilter(innerExpr)};

        if (!innerToken)
            return std::nullopt;

        // Create negated filter using embedded filter composition
        Token result;
        result.kind = Token::Kind::Filter;
        result.key  = QString("!(%1)").arg(innerExpr);

        // Embed a negated filter that inverts the result of the inner filter
        result.embedFilter(
            [innerToken = *innerToken](const QJsonValue& value) -> bool
            {
                auto innerResult{innerToken.evaluateEmbeddedFilter(value)};
                return !innerResult;
            });

        return result;
    }
    return std::nullopt;
}

std::optional<Token> parseEmbeddedFunction(const QString& s)
{
    // ============================================================================
    // Rule-Based Function Evaluation Dispatch System
    // ============================================================================

    // Evaluation rule types for side evaluation strategies
    enum class EvaluationRuleType : uint8_t
    {
        FunctionCall,   // Function call evaluation (length, count, etc.)
        PropertyAccess, // Property access (@.property)
        LiteralValue    // JSON literal value
    };

    // Comparison rule types for operator dispatch
    enum class ComparisonRuleType : uint8_t
    {
        Equality,    // == operator
        Inequality,  // != operator
        LessThan,    // < operator
        GreaterThan, // > operator
        LessEqual,   // <= operator
        GreaterEqual // >= operator
    };

    // Result structure for expression parsing
    struct FunctionExpressionComponents
    {
        QString left;
        QString operator_str;
        QString right;
        bool    leftHasFunc;
        bool    rightHasFunc;
        bool    needsRootContext;
    };

    // Result structure for side evaluation
    struct SideEvaluationResult
    {
        QJsonValue value;
        bool       isNothing;
        bool       success;
    };

    // Parse function comparison expression into components
    auto parseFunctionExpression = [](const QString& s) -> FunctionExpressionComponents
    {
        constexpr auto funcCompPat = ctll::fixed_string{R"(^(.*?)\s*(==|!=|<|>|<=|>=)\s*(.*?)$)"};

        if (auto m = ctre::match<funcCompPat>(to_sv(s)))
        {
            auto left  = to_qt_s(m.template get<1>().to_view()).trimmed();
            auto op    = to_qt_s(m.template get<2>().to_view());
            auto right = to_qt_s(m.template get<3>().to_view()).trimmed();

            auto leftHasFunc  = left.contains("(") && left.contains(")");
            auto rightHasFunc = right.contains("(") && right.contains(")");

            // Check if any function call needs root context (value($...))
            auto needsRootContext = false;
            if (leftHasFunc && left.contains("value($"))
            {
                qCDebug(jsonPathLog) << "Left side contains value($...): " << left;
                needsRootContext = true;
            }
            if (rightHasFunc && right.contains("value($"))
            {
                qCDebug(jsonPathLog) << "Right side contains value($...): " << right;
                needsRootContext = true;
            }

            qCDebug(jsonPathLog) << "needsRootContext=" << needsRootContext << "left=" << left << "right=" << right;

            return {left, op, right, leftHasFunc, rightHasFunc, needsRootContext};
        }

        return {"", "", "", false, false, false};
    };

    // Determine evaluation rule type for expression side
    auto determineEvaluationRuleType = [](const QString& expr, bool hasFunc) -> EvaluationRuleType
    {
        if (hasFunc)
            return EvaluationRuleType::FunctionCall;
        if (expr.startsWith("@."))
            return EvaluationRuleType::PropertyAccess;
        return EvaluationRuleType::LiteralValue;
    };

    // Evaluate expression side using appropriate strategy
    auto evaluateExpressionSide =
        [&](const QString& expr, bool hasFunc, const QJsonValue& node, const QJsonValue& root) -> SideEvaluationResult
    {
        auto ruleType = determineEvaluationRuleType(expr, hasFunc);

        switch (ruleType)
        {
        case EvaluationRuleType::FunctionCall:
        {
            QJsonValue result;
            if (expr.contains("value($"))
                result = evaluateFunction(expr, root);
            else
                result = evaluateFunction(expr, node);
            return {result, result.isUndefined(), true};
        }

        case EvaluationRuleType::PropertyAccess:
        {
            auto prop   = expr.mid(2);
            auto val    = node.toObject().value(prop);
            auto result = val.isUndefined() ? QJsonValue() : val;
            return {result, result.isUndefined(), true};
        }

        case EvaluationRuleType::LiteralValue:
        {
            auto result = parseJsonLiteral(expr);
            return {result, result.isUndefined(), true};
        }
        }

        return {QJsonValue(), true, false};
    };

    // Determine comparison rule type from operator string
    auto determineComparisonRuleType = [](const QString& op) -> ComparisonRuleType
    {
        if (op == "==")
            return ComparisonRuleType::Equality;
        if (op == "!=")
            return ComparisonRuleType::Inequality;
        if (op == "<")
            return ComparisonRuleType::LessThan;
        if (op == ">")
            return ComparisonRuleType::GreaterThan;
        if (op == "<=")
            return ComparisonRuleType::LessEqual;
        if (op == ">=")
            return ComparisonRuleType::GreaterEqual;
        return ComparisonRuleType::Equality; // Default fallback
    };

    // Perform comparison with RFC 9535 "Nothing" semantics
    auto performNothingAwareComparison =
        [](ComparisonRuleType ruleType, const SideEvaluationResult& left, const SideEvaluationResult& right) -> bool
    {
        switch (ruleType)
        {
        case ComparisonRuleType::Equality:
            if (left.isNothing && right.isNothing)
                return true; // Nothing == Nothing
            if (left.isNothing || right.isNothing)
                return false; // Nothing != any value
            return left.value == right.value;

        case ComparisonRuleType::Inequality:
            if (left.isNothing && right.isNothing)
                return false; // Nothing == Nothing
            if (left.isNothing || right.isNothing)
                return true; // Nothing != any value
            return left.value != right.value;

        case ComparisonRuleType::LessThan:
            return compareValues(left.value, right.value) < 0;

        case ComparisonRuleType::GreaterThan:
            return compareValues(left.value, right.value) > 0;

        case ComparisonRuleType::LessEqual:
            return compareValues(left.value, right.value) <= 0;

        case ComparisonRuleType::GreaterEqual:
            return compareValues(left.value, right.value) >= 0;
        }

        return false;
    };

    // Function Evaluation Rule Dispatcher
    struct FunctionEvaluationDispatcher
    {
        // Create context filter for root context evaluation
        static std::function<bool(const QJsonValue&, const QJsonValue&)> createContextFilter(
            const FunctionExpressionComponents& components,
            const std::function<SideEvaluationResult(const QString&, bool, const QJsonValue&, const QJsonValue&)>&
                                                                     evaluateExpressionSide,
            const std::function<ComparisonRuleType(const QString&)>& determineComparisonRuleType,
            const std::function<bool(ComparisonRuleType, const SideEvaluationResult&, const SideEvaluationResult&)>&
                performNothingAwareComparison)
        {
            return [components, evaluateExpressionSide, determineComparisonRuleType, performNothingAwareComparison](
                       const QJsonValue& node, const QJsonValue& root) -> bool
            {
                auto leftResult     = evaluateExpressionSide(components.left, components.leftHasFunc, node, root);
                auto rightResult    = evaluateExpressionSide(components.right, components.rightHasFunc, node, root);
                auto comparisonType = determineComparisonRuleType(components.operator_str);

                return performNothingAwareComparison(comparisonType, leftResult, rightResult);
            };
        }

        // Create regular filter for non-root context evaluation
        static std::function<bool(const QJsonValue&)> createRegularFilter(
            const QString& expr,
            const std::function<SideEvaluationResult(const QString&, bool, const QJsonValue&, const QJsonValue&)>&
                                                                     evaluateExpressionSide,
            const std::function<ComparisonRuleType(const QString&)>& determineComparisonRuleType,
            const std::function<bool(ComparisonRuleType, const SideEvaluationResult&, const SideEvaluationResult&)>&
                performNothingAwareComparison)
        {
            return [expr, evaluateExpressionSide, determineComparisonRuleType, performNothingAwareComparison](
                       const QJsonValue& node) -> bool
            {
                // Re-parse the expression at runtime to avoid capture issues.
                // IMPORTANT: Do NOT match against a temporary std::string, as CTRE's match object
                // holds views into the provided buffer. Use QStringView backed by the captured
                // QString to ensure stable storage during this invocation.
                constexpr auto funcCompPat = ctll::fixed_string{"^(.*?)\\s*(==|!=|<|>|<=|>=)\\s*(.*?)$"};
                auto           m           = ctre::match<funcCompPat>(to_sv(expr));
                if (!m)
                    return false;

                auto left  = to_qt_s(m.template get<1>().to_view());
                auto op    = to_qt_s(m.template get<2>().to_view());
                auto right = to_qt_s(m.template get<3>().to_view());

                auto leftHasFunc =
                    ctre::search<ctll::fixed_string{R"(\b(length|count|match|search|value)\s*\()"}>(to_sv(left));
                auto rightHasFunc =
                    ctre::search<ctll::fixed_string{R"(\b(length|count|match|search|value)\s*\()"}>(to_sv(right));

                auto leftResult     = evaluateExpressionSide(left, leftHasFunc, node, node);
                auto rightResult    = evaluateExpressionSide(right, rightHasFunc, node, node);
                auto comparisonType = determineComparisonRuleType(op);

                return performNothingAwareComparison(comparisonType, leftResult, rightResult);
            };
        }
    };

    // Main dispatch logic
    auto components = parseFunctionExpression(s);

    // Validate that we have a valid function expression
    if (components.operator_str.isEmpty() || (!components.leftHasFunc && !components.rightHasFunc))
        return std::nullopt;

    Token result;
    result.kind  = Token::Kind::Filter;
    result.index = 0;
    result.hash  = 0;
    result.key   = s;

    if (components.needsRootContext)
    {
        result.embedContextFilter(FunctionEvaluationDispatcher::createContextFilter(
            components, evaluateExpressionSide, determineComparisonRuleType, performNothingAwareComparison));
    }
    else
    {
        result.embedFilter(FunctionEvaluationDispatcher::createRegularFilter(
            s, evaluateExpressionSide, determineComparisonRuleType, performNothingAwareComparison));
    }

    return result;
}

} // namespace json_query::json_path::detail
