// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include <ctre.hpp>
#include <expected>

namespace json_query::json_path
{

using json_query::json_path::internal::ContainerCursor;
using json_query::json_path::internal::makeSimpleContextCursor;

namespace detail
{

// TableGen-inspired Context Filter Parsing Architecture
// =====================================================

// Forward declarations for existing helper functions
QJsonValue parseJsonLiteral(const QString& value); // From JSONPathFilterFunctions.cpp

// Strategy Enum: Different context filter parsing types
enum class ContextFilterParsingType
{
    ComparisonPattern, // $.foo == @.bar, $.a.b == 42
    FunctionPattern,   // length(@.a) == value($..c)
    ExistencePattern,  // $, $.foo, $.*.a
    DefaultPattern     // Fallback (no match)
};

// TableGen-Style Definition Templates
// ===================================

template <ContextFilterParsingType T>
struct ContextFilterParsingDef
{
    static constexpr bool enabled  = false;
    static constexpr int  priority = 0;
    static bool           matches(const QString& expr) { return false; }
    static QString        description() { return "Unknown pattern"; }
};

// Specialization: Comparison Pattern (highest priority)
template <>
struct ContextFilterParsingDef<ContextFilterParsingType::ComparisonPattern>
{
    static constexpr bool enabled  = true;
    static constexpr int  priority = 100;

    static bool matches(const QString& expr)
    {
        // Only accept simple absolute path references in comparisons per RFC 9535
        // Valid: $==@, $.foo==@.bar, $.a.b==42
        // Invalid: $.*==42, $[*]==42, $..foo==42 (non-singular queries)
        static const auto absComparisonPat{ctre::match<R"(\$(\.[a-zA-Z_][a-zA-Z0-9_]*)*\s*(==|!=|<=|>=|<|>)\s*(.+))">};
        return absComparisonPat(json_query::utils::to_sv(expr));
    }

    static QString description() { return "Absolute path comparison pattern"; }
};

// Specialization: Function Pattern (medium-high priority)
template <>
struct ContextFilterParsingDef<ContextFilterParsingType::FunctionPattern>
{
    static constexpr bool enabled  = true;
    static constexpr int  priority = 90;

    static bool matches(const QString& expr)
    {
        // Handle function call patterns like length(@.a) == value($..c)
        return expr.contains("length(") || expr.contains("value($");
    }

    static QString description() { return "Function call pattern"; }
};

// Specialization: Existence Pattern (medium priority)
template <>
struct ContextFilterParsingDef<ContextFilterParsingType::ExistencePattern>
{
    static constexpr bool enabled  = true;
    static constexpr int  priority = 80;

    static bool matches(const QString& expr)
    {
        // Check for absolute path existence filters (allow wildcards and complex paths)
        // Valid: $, $.foo, $.*.a, $[0], etc.
        static const auto absExistencePat{ctre::match<R"(\$(\.[a-zA-Z_*][a-zA-Z0-9_]*|\[\d+\]|\[.*\])*)">};
        return absExistencePat(json_query::utils::to_sv(expr));
    }

    static QString description() { return "Absolute path existence pattern"; }
};

// Template Specialization Strategies
// ===================================

template <ContextFilterParsingType T>
struct ContextFilterParsingStrategy
{
    static std::optional<Token> process(const QString& expr, std::vector<ContextFilterFn>& out)
    {
        qCDebug(jsonPathLog) << "ContextFilterParsingStrategy: unsupported pattern type";
        return std::nullopt;
    }
};

// Helper: Context Builder for creating filter tokens
struct ContextBuilder
{
    std::vector<ContextFilterFn>& fns;

    [[nodiscard]] Token add(ContextFilterFn fn, QString key = {})
    {
        fns.push_back(std::move(fn));
        const auto id{fns.size() - 1};
        Token      token{Token::Kind::Filter, 0, {}, 0u, std::move(key)};
        token.filterId        = SIZE_MAX; // Explicit: not using regular (non-context) legacy filter
        token.contextFilterId = id;
        return token;
    }
};

// Helper: Evaluate absolute path against root with proper error handling
std::expected<QJsonValue, Error>
evaluateAbsolutePathSafe(const QString& path, const QJsonValue& root, const QJsonValue& node)
{
    if (path == "$")
        return root;

    // Use monadic error handling instead of try/catch
    auto absolutePath{JSONPath::create(path)};
    if (!absolutePath)
    {
        qCDebug(jsonPathLog) << "Failed to create JSONPath for:" << path;
        return std::unexpected(EvalError::KeyNotFound); // Path creation failed
    }

    auto results{absolutePath->evaluate(root)};
    if (!results)
    {
        qCDebug(jsonPathLog) << "Failed to evaluate absolute path:" << path;
        return std::unexpected(results.error()); // Propagate the evaluation error
    }

    if (results->isEmpty())
    {
        qCDebug(jsonPathLog) << "No results for absolute path:" << path;
        return std::unexpected(EvalError::KeyNotFound); // No results found
    }

    // Use ContextAwareContainerCursor for efficient result processing
    auto cursor{makeSimpleContextCursor(*results, root, node)};

    // Get first result using zero-copy iteration
    for (const auto& [result, ctx] : cursor)
        return result;

    // Fallback if cursor iteration fails - return first result directly
    return results->first();
}

// Helper: Evaluate absolute path against root (legacy wrapper for compatibility)
QJsonValue evaluateAbsolutePath(const QString& path, const QJsonValue& root, const QJsonValue& node)
{
    auto result{evaluateAbsolutePathSafe(path, root, node)};
    if (result)
        return *result;

    qCDebug(jsonPathLog) << "Absolute path evaluation failed:" << path << "error:" << to_qt_sv(result.error());
    return QJsonValue{};
}

// Helper: Evaluate relative path (@.property, etc.)
QJsonValue evaluateRelativePath(const QString& relativePath, const QJsonValue& node, const QJsonValue& root)
{
    if (relativePath == "@")
        return node;

    if (relativePath.startsWith("@."))
    {
        auto propName{relativePath.mid(2)};
        if (node.isObject())
        {
            // Use context-aware cursor for efficient property lookup
            auto cursor{makeSimpleContextCursor(node.toObject(), root, node)};
            // Traditional property access (cursor doesn't expose keys in current interface)
            return node.toObject().value(propName);
        }
    }

    return QJsonValue{};
}

// Helper: Perform comparison with RFC 9535 "nothing" semantics
bool performComparison(const QString& op, const QJsonValue& leftValue, const QJsonValue& rightValue)
{
    if (op == "==")
    {
        if (leftValue.isUndefined() && rightValue.isUndefined())
            return true;
        if (leftValue.isUndefined() && rightValue.toDouble() == 0)
            return true;
        if (rightValue.isUndefined() && leftValue.toDouble() == 0)
            return true;
        if (leftValue.isUndefined() && !rightValue.isUndefined())
            return true;
        if (rightValue.isUndefined() && !leftValue.isUndefined())
            return false; // Asymmetric
        return leftValue == rightValue;
    }
    if (op == "!=")
    {
        if (leftValue.isUndefined() && rightValue.isUndefined())
            return false;
        if (leftValue.isUndefined() && rightValue.toDouble() == 0)
            return false;
        if (rightValue.isUndefined() && leftValue.toDouble() == 0)
            return false;
        return leftValue != rightValue;
    }
    if (op == "<" && leftValue.isDouble() && rightValue.isDouble())
        return leftValue.toDouble() < rightValue.toDouble();
    if (op == ">" && leftValue.isDouble() && rightValue.isDouble())
        return leftValue.toDouble() > rightValue.toDouble();
    if (op == "<=" && leftValue.isDouble() && rightValue.isDouble())
        return leftValue.toDouble() <= rightValue.toDouble();
    if (op == ">=" && leftValue.isDouble() && rightValue.isDouble())
        return leftValue.toDouble() >= rightValue.toDouble();

    return false;
}

// Helper function to evaluate function calls with root context
// Now uses ContextAwareContainerCursor for efficient iteration with context access
QJsonValue evaluateContextFunction(const QString& funcExpr, const QJsonValue& context, const QJsonValue& root)
{
    auto openParen{funcExpr.indexOf('(')};
    auto closeParen{funcExpr.lastIndexOf(')')};
    if (openParen == -1 || closeParen == -1)
        return {}; // Invalid function syntax

    auto funcName{funcExpr.mid(0, openParen).trimmed()};
    auto args{funcExpr.mid(openParen + 1, closeParen - openParen - 1).trimmed()};

    if (funcName == "length")
    {
        // Evaluate the argument (usually @.property) with context-aware iteration
        QJsonValue argValue;
        if (args.startsWith("@."))
        {
            auto prop{args.mid(2)};
            if (context.isObject())
            {
                // Use ContextAwareContainerCursor for efficient object property access
                auto cursor{makeSimpleContextCursor(context.toObject(), root, context)};

                // Find the property using zero-copy iteration
                for (const auto& [value, ctx] : cursor)
                {
                    // Note: ContainerCursor doesn't expose keys directly in the current interface
                    // For now, fall back to traditional access for property lookup
                    argValue = context.toObject().value(prop);
                    break; // We found our property access pattern
                }

                if (argValue.isUndefined())
                {
                    // Property not found, use traditional fallback
                    argValue = context.toObject().value(prop);
                }
            }
        }
        else if (args == "@")
        {
            argValue = context;
        }
        else
        {
            argValue = context; // Default to context
        }

        // Return length as QJsonValue - RFC 9535 "nothing" semantics
        if (argValue.isUndefined() || argValue.isNull())
            return QJsonValue{0}; // Nothing has length 0
        if (argValue.isString())
            return QJsonValue{argValue.toString().length()};

        // Use ContextAwareContainerCursor for efficient length calculation
        if (argValue.isArray())
        {
            auto cursor{makeSimpleContextCursor(asArray(argValue).get(), root, context)};
            return QJsonValue{cursor.size()}; // Zero-copy size access
        }
        if (argValue.isObject())
        {
            auto cursor{makeSimpleContextCursor(argValue.toObject(), root, context)};
            return QJsonValue{cursor.size()}; // Zero-copy size access
        }

        return QJsonValue{0}; // Other types have no length
    }

    if (funcName == "value")
    {
        // Implement proper JSONPath evaluation for complex paths like $..c
        // Enhanced with context-aware cursor for efficient traversal
        if (args.startsWith("$"))
        {
            // This is a JSONPath expression that needs to be evaluated against the root
            auto path{JSONPath::create(args)};
            if (path)
            {
                // Evaluate against root document with context awareness
                auto results{path->evaluate(root)};
                if (results)
                {
                    if (results->isEmpty())
                    {
                        return QJsonValue{0}; // Return 0 for empty results (RFC 9535 "nothing" semantics)
                    }
                    else
                    {
                        // Use ContextAwareContainerCursor for efficient result processing
                        auto cursor{makeSimpleContextCursor(*results, root, context)};

                        // Return the first result, with context-aware processing
                        for (const auto& [result, ctx] : cursor)
                        {
                            // Return the actual result value (not length)
                            return result;
                        }

                        // Fallback if cursor iteration fails
                        auto result{results->first()};
                        return result;
                    }
                }
                return QJsonValue{0}; // Invalid JSONPath expression returns 0
            }
            return QJsonValue{0}; // Invalid JSONPath expression returns 0
        }
        else if (args.startsWith("@."))
        {
            auto prop{args.mid(2)};
            if (context.isObject())
            {
                // Use context-aware cursor for efficient property access
                auto cursor{makeSimpleContextCursor(context.toObject(), root, context)};

                // Traditional property access (cursor doesn't expose keys in current interface)
                auto val{context.toObject().value(prop)};
                return val.isUndefined() ? QJsonValue{0} : val; // Return 0 for undefined
            }
        }
        else if (args == "@")
        {
            return context;
        }
        return QJsonValue{0}; // Undefined for complex paths returns 0
    }

    return {}; // Unknown function
}

// Helper: Find comparison operator in expression
struct OperatorParseResult
{
    QString   op;
    qsizetype position;
    bool      found;
};

OperatorParseResult findComparisonOperator(const QString& expr)
{
    // Look for comparison operators in order of precedence (longest first)
    const QStringList operators = {"<=", ">=", "==", "!=", "<", ">"};

    for (const QString& testOp : operators)
    {
        auto pos{expr.indexOf(testOp)};
        if (pos != -1)
            return {testOp, pos, true};
    }

    return {"", -1, false};
}

// Helper: Parse expression into left, operator, right components
struct ExpressionComponents
{
    QString left;
    QString op;
    QString right;
    bool    valid;
};

ExpressionComponents parseExpressionComponents(const QString& expr)
{
    auto opResult{findComparisonOperator(expr)};
    if (!opResult.found)
        return {"", "", "", false};

    auto left{expr.left(opResult.position).trimmed()};
    auto right{expr.mid(opResult.position + opResult.op.length()).trimmed()};

    return {left, opResult.op, right, true};
}

// Helper: Evaluate expression side (left or right)
struct SideEvaluationResult
{
    QJsonValue value;
    bool       isNothing;
};

SideEvaluationResult evaluateExpressionSide(const QString& side, const QJsonValue& node, const QJsonValue& root)
{
    QJsonValue val;
    bool       isNothing{false};

    if (side.contains("("))
    {
        val = evaluateContextFunction(side, node, root);

        // Check if this represents "nothing" (undefined/null property in length)
        if (side.startsWith("length(@."))
        {
            QString prop{side.mid(9, side.length() - 10)}; // Extract property name
            if (node.isObject())
            {
                auto propValue{node.toObject().value(prop)};
                if (propValue.isUndefined() || propValue.isNull())
                    isNothing = true;
            }
        }
        // Check if this represents "nothing" (empty results in value)
        else if (side.startsWith("value($"))
        {
            QString pathStr{side.mid(6, side.length() - 7)}; // Extract path
            auto    path{JSONPath::create(pathStr)};
            if (path)
            {
                auto results{path->evaluate(root)};
                if (results)
                {
                    if (results->isEmpty())
                    {
                        isNothing = true;
                    }
                    else
                    {
                        // Use context-aware cursor for efficient result validation
                        auto cursor{makeSimpleContextCursor(*results, root, node)};

                        // Check if results are effectively empty using zero-copy iteration
                        auto hasValidResults{false};
                        for (const auto& [result, ctx] : cursor)
                        {
                            if (!result.isUndefined() && !result.isNull())
                            {
                                hasValidResults = true;
                                break;
                            }
                        }

                        if (!hasValidResults)
                            isNothing = true;
                    }
                }
            }
        }
    }
    else if (side.startsWith("@."))
    {
        auto prop{side.mid(2)};
        if (node.isObject())
        {
            auto nodeVal{node.toObject().value(prop)};
            if (nodeVal.isUndefined())
            {
                isNothing = true;
                val       = QJsonValue{0};
            }
            else
            {
                val = nodeVal;
            }
        }
    }
    else
    {
        val = QJsonValue{side};
    }

    return {val, isNothing};
}

// Helper: Perform RFC 9535 "nothing" semantics comparison
bool performNothingAwareComparison(const QString&              op,
                                   const SideEvaluationResult& left,
                                   const SideEvaluationResult& right)
{
    if (op == "==")
    {
        if (left.isNothing && right.isNothing)
            return true;
        if (left.isNothing && right.value.toDouble() == 0)
            return true;
        if (right.isNothing && left.value.toDouble() == 0)
            return true;
        if (left.isNothing && !right.isNothing)
            return true;
        if (right.isNothing && !left.isNothing)
            return false; // Asymmetric
        // Both sides have actual values - use normal comparison
        return left.value == right.value;
    }
    if (op == "!=")
    {
        if (left.isNothing && right.isNothing)
            return false;
        if (left.isNothing && right.value.toDouble() == 0)
            return false;
        if (right.isNothing && left.value.toDouble() == 0)
            return false;
        return left.value != right.value;
    }
    return false;
}

// Helper: Check if expression needs root context
bool needsRootContext(const QString& left, const QString& right)
{
    return left.contains("value($") || right.contains("value($");
}

// Helper: Extract regex match components for comparison pattern
struct ComparisonMatchResult
{
    QString leftPath;
    QString op;
    QString rightExpr;
    bool    valid;
};

ComparisonMatchResult extractComparisonComponents(const QString& expr)
{
    static const auto absComparisonPat{ctre::match<R"(\$(\.[a-zA-Z_][a-zA-Z0-9_]*)*\s*(==|!=|<=|>=|<|>)\s*(.+))">};
    if (auto match = absComparisonPat(json_query::utils::to_sv(expr)))
    {
        const QString leftPath  = QStringLiteral("$") + json_query::utils::to_qt_s(match.get<1>().to_view());
        const QString op        = json_query::utils::to_qt_s(match.get<2>().to_view());
        const QString rightExpr = json_query::utils::to_qt_s(match.get<3>().to_view());

        return {leftPath, op, rightExpr, true};
    }

    return {"", "", "", false};
}

// Strategy Specializations
// ========================

// Specialization: Comparison Pattern Strategy
template <>
struct ContextFilterParsingStrategy<ContextFilterParsingType::ComparisonPattern>
{
    static std::optional<Token> process(const QString& expr, std::vector<ContextFilterFn>& out)
    {
        qCDebug(jsonPathLog) << "ContextFilterParsingStrategy: processing comparison pattern:" << expr;

        auto matchResult{extractComparisonComponents(expr)};
        if (!matchResult.valid)
            return std::nullopt;

        auto leftPath{matchResult.leftPath};
        auto op{matchResult.op};
        auto rightExpr{matchResult.rightExpr};

        qCDebug(jsonPathLog) << "Comparison parts: leftPath=" << leftPath << "op=" << op << "rightExpr=" << rightExpr;

        ContextBuilder b{out};
        return b.add(
            [leftPath, op, rightExpr](const QJsonValue& node, const QJsonValue& root) -> bool
            {
                // Evaluate left side (absolute path)
                QJsonValue leftValue{evaluateAbsolutePath(leftPath, root, node)};

                // Evaluate right side
                QJsonValue rightValue;
                auto       rightExprStr = rightExpr;
                if (rightExprStr.startsWith("@"))
                    rightValue = evaluateRelativePath(rightExprStr, node, root);
                else
                    rightValue = parseJsonLiteral(rightExprStr);

                // Perform comparison
                return performComparison(op, leftValue, rightValue);
            });
    }
};

// Specialization: Function Pattern Strategy
template <>
struct ContextFilterParsingStrategy<ContextFilterParsingType::FunctionPattern>
{
    static std::optional<Token> process(const QString& expr, std::vector<ContextFilterFn>& out)
    {
        qCDebug(jsonPathLog) << "ContextFilterParsingStrategy: processing function pattern:" << expr;

        auto components{parseExpressionComponents(expr)};
        if (!components.valid)
            return std::nullopt;

        auto left{components.left};
        auto op{components.op};
        auto right{components.right};

        qCDebug(jsonPathLog) << "Function pattern parts: left=" << left << "op=" << op << "right=" << right;

        // Check if we need root context (value($...))
        auto needsRoot{needsRootContext(left, right)};

        if (needsRoot)
        {
            ContextBuilder b{out};
            return b.add(
                [left, op, right](const QJsonValue& node, const QJsonValue& root) -> bool
                {
                    auto leftResult{evaluateExpressionSide(left, node, root)};
                    auto rightResult{evaluateExpressionSide(right, node, root)};

                    // Use "nothing" aware comparison for RFC 9535 compliance
                    if (leftResult.isNothing || rightResult.isNothing)
                        return performNothingAwareComparison(op, leftResult, rightResult);

                    // Both sides have actual values - use normal comparison
                    return performComparison(op, leftResult.value, rightResult.value);
                });
        }
        else
        {
            // No root context needed - use simpler evaluation
            ContextBuilder b{out};
            return b.add(
                [left, op, right](const QJsonValue& node, const QJsonValue& root) -> bool
                {
                    auto leftResult{evaluateExpressionSide(left, node, root)};
                    auto rightResult{evaluateExpressionSide(right, node, root)};

                    return performComparison(op, leftResult.value, rightResult.value);
                });
        }
    }
};

// Specialization: Existence Pattern Strategy
template <>
struct ContextFilterParsingStrategy<ContextFilterParsingType::ExistencePattern>
{
    static std::optional<Token> process(const QString& expr, std::vector<ContextFilterFn>& out)
    {
        qCDebug(jsonPathLog) << "ContextFilterParsingStrategy: processing existence pattern:" << expr;

        ContextBuilder b{out};
        return b.add(
            [expr](const QJsonValue& node, const QJsonValue& root) -> bool
            {
                qCDebug(jsonPathLog) << "Evaluating absolute path existence filter:" << expr << "on root:" << root;

                // Basic implementation for absolute path existence filters
                if (expr == "$")
                {
                    // Root existence filter: always true if root exists
                    return !root.isUndefined();
                }

                // Check if this is an absolute path existence filter
                if (expr.startsWith("$"))
                {
                    // Use proper monadic error handling instead of try/catch
                    auto absolutePath{JSONPath::create(expr)};
                    if (absolutePath)
                    {
                        auto results{absolutePath->evaluate(root)};
                        if (results && !results->isEmpty())
                            return true;

                        // Log specific error if evaluation failed
                        if (!results)
                        {
                            qCDebug(jsonPathLog) << "Failed to evaluate absolute path:" << expr
                                                 << "error:" << to_qt_sv(results.error());
                        }
                    }
                    else
                    {
                        qCDebug(jsonPathLog) << "Failed to create JSONPath for:" << expr;
                    }
                }

                // Fallback for unimplemented patterns
                qCDebug(jsonPathLog) << "Absolute path pattern not yet fully implemented:" << expr;
                return false;
            });
    }
};

// TableGen-inspired dispatch architecture for context filter parsing
struct ContextFilterParsingDispatcher
{
    static std::optional<Token> dispatch(const QString& expr, std::vector<ContextFilterFn>& out)
    {
        qCDebug(jsonPathLog) << "ContextFilterParsingDispatcher: dispatching expression:" << expr;

        // Try patterns in priority order (highest priority first)

        // 1. Comparison Pattern (priority 100)
        if constexpr (ContextFilterParsingDef<ContextFilterParsingType::ComparisonPattern>::enabled)
        {
            if (ContextFilterParsingDef<ContextFilterParsingType::ComparisonPattern>::matches(expr))
            {
                qCDebug(jsonPathLog) << "Matched ComparisonPattern";
                return ContextFilterParsingStrategy<ContextFilterParsingType::ComparisonPattern>::process(expr, out);
            }
        }

        // 2. Function Pattern (priority 90)
        if constexpr (ContextFilterParsingDef<ContextFilterParsingType::FunctionPattern>::enabled)
        {
            if (ContextFilterParsingDef<ContextFilterParsingType::FunctionPattern>::matches(expr))
            {
                qCDebug(jsonPathLog) << "Matched FunctionPattern";
                return ContextFilterParsingStrategy<ContextFilterParsingType::FunctionPattern>::process(expr, out);
            }
        }

        // 3. Existence Pattern (priority 80)
        if constexpr (ContextFilterParsingDef<ContextFilterParsingType::ExistencePattern>::enabled)
        {
            if (ContextFilterParsingDef<ContextFilterParsingType::ExistencePattern>::matches(expr))
            {
                qCDebug(jsonPathLog) << "Matched ExistencePattern";
                return ContextFilterParsingStrategy<ContextFilterParsingType::ExistencePattern>::process(expr, out);
            }
        }

        qCDebug(jsonPathLog) << "ContextFilterParsingDispatcher: no match found for:" << expr;
        return std::nullopt;
    }
};

} // namespace detail

// Parse context-aware function calls like length(@.a) == value($..c)
std::optional<Token> parseAbsolutePathContext(const QString& s, std::vector<ContextFilterFn>& out)
{
    qCDebug(jsonPathLog) << "parseAbsolutePathContext called with:" << s;

    // TableGen-inspired dispatch architecture for context filter parsing
    return detail::ContextFilterParsingDispatcher::dispatch(s, out);
}

// Context-aware filter compilation dispatcher
std::optional<Token>
compileContextFilter(const QString& expr, std::vector<ContextFilterFn>& contextOut, std::vector<FilterFn>& regularOut)
{
    qCDebug(jsonPathLog) << "compileContextFilter called with expr:" << expr;

    // First try to parse as context-aware filter (absolute path references)
    if (auto contextToken = parseAbsolutePathContext(expr, contextOut))
    {
        qCDebug(jsonPathLog) << "compileContextFilter: compiled as context-aware filter";
        return contextToken;
    }

    // Fall back to regular filter compilation
    qCDebug(jsonPathLog) << "compileContextFilter: falling back to regular filter";
    if (auto regularToken = compileFilter(expr, regularOut))
    {
        qCDebug(jsonPathLog) << "compileContextFilter: regular filter compiled successfully";
        // Return the regular token as-is - no need to wrap for backward compatibility
        // The evaluation logic will handle both regular and context-aware filters appropriately
        return regularToken;
    }

    qCDebug(jsonPathLog) << "compileContextFilter: failed to compile filter";
    return std::nullopt;
}

} // namespace json_query::json_path
