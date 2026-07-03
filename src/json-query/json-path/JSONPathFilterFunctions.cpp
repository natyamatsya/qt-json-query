// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-path/JSONPathFilter.hpp"
#include "json-query/json-path/JSONPathFilterParsers.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include <QJsonDocument>
#include <QJsonParseError>
#include <ctre.hpp>

namespace json_query::json_path::detail
{

using json_query::json_path::detail::parseJsonLiteral;
using json_query::utils::to_qt_s;
using json_query::utils::to_sv;

// Helper function to evaluate function calls like length(@.a) or value($..c)
QJsonValue evaluateFunction(const QString& funcCall, const QJsonValue& context)
{
    // Refactored to use explicit error handling for JSONPath evaluation
    auto parseFunctionSyntax = [](const QString& expr) -> std::optional<std::pair<QString, QString>>
    {
        auto openParen{expr.indexOf('(')};
        auto closeParen{expr.lastIndexOf(')')};
        if (openParen == -1 || closeParen == -1)
            return std::nullopt; // Invalid function syntax

        const auto funcName{expr.mid(0, openParen).trimmed()};
        const auto args{expr.mid(openParen + 1, closeParen - openParen - 1).trimmed()};
        return std::make_pair(QString(funcName), QString(args));
    };

    // Evaluate argument value
    auto evaluateArgument = [&context](const QString& args) -> QJsonValue
    {
        if (args.startsWith("@."))
        {
            const auto prop{args.mid(2)};
            return context.toObject().value(prop);
        }
        else if (args == "@")
        {
            return context;
        }
        return context; // Default to context
    };

    // Calculate length
    auto calculateLength = [](const QJsonValue& value) -> QJsonValue
    {
        if (value.isUndefined() || value.isNull())
            return {}; // Return Nothing for undefined/null
        if (value.isString())
            return QJsonValue{value.toString().length()};
        if (value.isArray())
            return QJsonValue{value.toArray().size()};
        if (value.isObject())
            return QJsonValue{value.toObject().size()};
        return {}; // Return Nothing for other types (RFC 9535)
    };

    // Evaluate JSONPath value
    auto evaluateJsonPathValue = [&context](const QString& args) -> QJsonValue
    {
        if (args.startsWith("$"))
        {
            auto pathResult{JSONPath::create(args)};
            if (!pathResult)
                return {}; // Return null for invalid JSONPath

            auto evalResult{pathResult->evaluate(context)};
            if (!evalResult)
                return {}; // Return null for evaluation errors

            // RFC 9535 "value" function semantics: return the value if single node, return Nothing if empty or
            // multiple nodes
            if (evalResult->isEmpty())
                return {}; // Return null for no results (RFC 9535 "nothing")

            if (evalResult->size() == 1)
                return (*evalResult)[0]; // Return the single value

            return {}; // Return null for multiple results (RFC 9535 "nothing")
        }
        else if (args.startsWith("@."))
        {
            const auto prop{args.mid(2)};
            auto       val{context.toObject().value(prop)};
            return val.isUndefined() ? QJsonValue() : val;
        }
        else if (args == "@")
        {
            return context;
        }
        return {}; // Undefined for complex paths
    };

    // Main function evaluation
    auto parsed{parseFunctionSyntax(funcCall)};
    if (!parsed)
        return {}; // Invalid function syntax

    const auto& [funcName, args] = *parsed;

    if (funcName == "length")
        return calculateLength(evaluateArgument(args));
    else if (funcName == "value")
        return evaluateJsonPathValue(args);
    return {}; // Unknown function
}

// Helper function for value comparison with type checking
int compareValues(const QJsonValue& left, const QJsonValue& right)
{
    // Handle null values
    if (left.isNull() && right.isNull())
        return 0;
    if (left.isNull())
        return -1;
    if (right.isNull())
        return 1;

    // Handle numbers
    if (left.isDouble() && right.isDouble())
    {
        auto l{left.toDouble()};
        auto r{right.toDouble()};
        if (l < r)
            return -1;
        if (l > r)
            return 1;
        return 0;
    }

    // Handle strings
    if (left.isString() && right.isString())
        return left.toString().compare(right.toString());

    // Handle booleans
    if (left.isBool() && right.isBool())
    {
        auto l{left.toBool()};
        auto r{right.toBool()};
        if (l == r)
            return 0;
        return l ? 1 : -1; // true > false
    }

    // Different types - use type ordering
    auto leftType  = static_cast<int>(left.type());
    auto rightType = static_cast<int>(right.type());
    return leftType - rightType;
}

// Parse function calls like length() and value() in filter expressions
std::optional<Token> parseFunction(const QString& s, std::vector<FilterFn>& out)
{
    // Pattern for function call comparisons: func(...) op value or value op func(...)
    constexpr auto funcCompPat = ctll::fixed_string{R"(^(.*?)\s*(==|!=|<|>|<=|>=)\s*(.*?)$)"};

    if (auto m = ctre::match<funcCompPat>(to_sv(s)))
    {
        const auto left{to_qt_s(m.template get<1>().to_view()).trimmed()};
        const auto op{to_qt_s(m.template get<2>().to_view())};
        const auto right{to_qt_s(m.template get<3>().to_view()).trimmed()};

        // Check if either side contains a function call
        auto leftHasFunc{left.contains("(") && left.contains(")")};
        auto rightHasFunc{right.contains("(") && right.contains(")")};

        if (!leftHasFunc && !rightHasFunc)
            return std::nullopt; // No function calls found

        // Check if any function call needs root context (value($...))
        auto needsRootContext{false};
        if (leftHasFunc && left.contains("value($"))
            needsRootContext = true;
        if (rightHasFunc && right.contains("value($"))
            needsRootContext = true;

        if (needsRootContext)
        {
            // This needs to be handled by context-aware filter infrastructure
            return std::nullopt;
        }

        Builder b{out};
        return b.add(
            [left, op, right, leftHasFunc, rightHasFunc](const QJsonValue& j) -> bool
            {
                QJsonValue leftVal, rightVal;

                // Evaluate left side
                if (leftHasFunc)
                {
                    leftVal = evaluateFunction(left, j);
                }
                else if (left.startsWith("@."))
                {
                    // Property access - RFC 9535 "nothing" semantics
                    const auto prop{left.mid(2)};
                    auto       val{j.toObject().value(prop)};
                    leftVal = val.isUndefined() ? QJsonValue{0} : val; // Undefined becomes 0
                }
                else
                {
                    leftVal = parseJsonLiteral(left);
                }

                // Evaluate right side
                if (rightHasFunc)
                {
                    rightVal = evaluateFunction(right, j);
                }
                else if (right.startsWith("@."))
                {
                    auto prop = right.mid(2);
                    auto val  = j.toObject().value(prop);
                    rightVal  = val.isUndefined() ? QJsonValue() : val;
                }
                else
                {
                    rightVal = parseJsonLiteral(right);
                }

                // Perform comparison
                if (op == "==")
                    return leftVal == rightVal;
                if (op == "!=")
                    return leftVal != rightVal;
                if (op == "<")
                    return compareValues(leftVal, rightVal) < 0;
                if (op == ">")
                    return compareValues(leftVal, rightVal) > 0;
                if (op == "<=")
                    return compareValues(leftVal, rightVal) <= 0;
                if (op == ">=")
                    return compareValues(leftVal, rightVal) >= 0;
                return false;
            },
            s);
    }

    return std::nullopt;
}

} // namespace json_query::json_path::detail
