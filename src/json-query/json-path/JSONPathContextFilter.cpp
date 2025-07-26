#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include <QDebug>
#include <iostream>
#include <ctre.hpp>

namespace json_query::json_path {

using json_query::json_path::internal::ContainerCursor;
using json_query::json_path::internal::makeSimpleContextCursor;

// Helper function to evaluate function calls with root context
// Now uses ContextAwareContainerCursor for efficient iteration with context access
QJsonValue evaluateContextFunction(const QString& funcExpr, const QJsonValue& context, const QJsonValue& root) {
    int openParen = funcExpr.indexOf('(');
    int closeParen = funcExpr.lastIndexOf(')');
    if (openParen == -1 || closeParen == -1) {
        return QJsonValue(); // Invalid function syntax
    }
    
    QString funcName = funcExpr.mid(0, openParen).trimmed();
    QString args = funcExpr.mid(openParen + 1, closeParen - openParen - 1).trimmed();
    
    if (funcName == "length") {
        // Evaluate the argument (usually @.property) with context-aware iteration
        QJsonValue argValue;
        if (args.startsWith("@.")) {
            QString prop = args.mid(2);
            if (context.isObject()) {
                // Use ContextAwareContainerCursor for efficient object property access
                auto cursor = makeSimpleContextCursor(context.toObject(), root, context);
                
                // Find the property using zero-copy iteration
                for (const auto& [value, ctx] : cursor) {
                    // Note: ContainerCursor doesn't expose keys directly in the current interface
                    // For now, fall back to traditional access for property lookup
                    argValue = context.toObject().value(prop);
                    break; // We found our property access pattern
                }
                
                if (argValue.isUndefined()) {
                    // Property not found, use traditional fallback
                    argValue = context.toObject().value(prop);
                }
            }
        } else if (args == "@") {
            argValue = context;
        } else {
            argValue = context; // Default to context
        }
        
        // Return length as QJsonValue - RFC 9535 "nothing" semantics
        if (argValue.isUndefined() || argValue.isNull()) return QJsonValue(0); // Nothing has length 0
        if (argValue.isString()) return QJsonValue(argValue.toString().length());
        
        // Use ContextAwareContainerCursor for efficient length calculation
        if (argValue.isArray()) {
            auto cursor = makeSimpleContextCursor(argValue.toArray(), root, context);
            return QJsonValue(cursor.size()); // Zero-copy size access
        }
        if (argValue.isObject()) {
            auto cursor = makeSimpleContextCursor(argValue.toObject(), root, context);
            return QJsonValue(cursor.size()); // Zero-copy size access
        }
        
        return QJsonValue(0); // Other types have no length
    }
    
    if (funcName == "value") {
        // Implement proper JSONPath evaluation for complex paths like $..c
        // Enhanced with context-aware cursor for efficient traversal
        if (args.startsWith("$")) {
            // This is a JSONPath expression that needs to be evaluated against the root
            using json_query::JSONPath;
            auto path = JSONPath::create(args);
            if (path) {
                // Evaluate against root document with context awareness
                auto results = path->evaluateAll(root);
                if (results) {
                    if (results->isEmpty()) {
                        return QJsonValue(0); // Return 0 for empty results (RFC 9535 "nothing" semantics)
                    } else {
                        // Use ContextAwareContainerCursor for efficient result processing
                        auto cursor = makeSimpleContextCursor(*results, root, context);
                        
                        // Return the first result, with context-aware processing
                        for (const auto& [result, ctx] : cursor) {
                            // Return the actual result value (not length)
                            return result;
                        }
                        
                        // Fallback if cursor iteration fails
                        QJsonValue result = results->first();
                        return result;
                    }
                }
                return QJsonValue(0); // Invalid JSONPath expression returns 0
            }
            return QJsonValue(0); // Invalid JSONPath expression returns 0
        } else if (args.startsWith("@.")) {
            QString prop = args.mid(2);
            if (context.isObject()) {
                // Use context-aware cursor for efficient property access
                auto cursor = makeSimpleContextCursor(context.toObject(), root, context);
                
                // Traditional property access (cursor doesn't expose keys in current interface)
                QJsonValue val = context.toObject().value(prop);
                return val.isUndefined() ? QJsonValue(0) : val; // Return 0 for undefined
            }
        } else if (args == "@") {
            return context;
        }
        return QJsonValue(0); // Undefined for complex paths returns 0
    }
    
    return QJsonValue(); // Unknown function
}

// Parse context-aware function calls like length(@.a) == value($..c)
std::optional<Token> parseAbsolutePathContext(QString s, QVector<ContextFilterFn>& out)
{
    qCDebug(jsonPathLog) << "parseAbsolutePathContext called with:" << s;
    
    // Check for absolute path comparison patterns first
    // Only accept simple absolute path references in comparisons per RFC 9535
    // Valid: $==@, $.foo==@.bar, $.a.b==42
    // Invalid: $.*==42, $[*]==42, $..foo==42 (non-singular queries)
    static const auto absComparisonPat = ctre::match<R"(\$(\.[a-zA-Z_][a-zA-Z0-9_]*)*\s*(==|!=|<=|>=|<|>)\s*(.+))">;
    if (auto match = absComparisonPat(s.toStdString())) {
        qCDebug(jsonPathLog) << "parseAbsolutePathContext: matched simple absolute path comparison:" << s;
        
        std::string leftPath = "$" + std::string(match.get<1>().to_view());
        std::string op = std::string(match.get<2>().to_view());
        std::string rightExpr = std::string(match.get<3>().to_view());
        
        qCDebug(jsonPathLog) << "parseAbsolutePathContext: leftPath=" << QString::fromStdString(leftPath) 
                             << "op=" << QString::fromStdString(op) 
                             << "rightExpr=" << QString::fromStdString(rightExpr);
        
        struct ContextBuilder {
            QVector<ContextFilterFn>& fns;
            
            [[nodiscard]] Token add(ContextFilterFn fn, QString key = {})
            {
                fns.push_back(std::move(fn));
                const std::size_t id = fns.size() - 1;
                Token token{Token::Kind::Filter, 0, {}, 0u, std::move(key)};
                token.contextFilterId = id;
                return token;
            }
        };
        
        ContextBuilder b{out};
        return b.add([leftPath, op, rightExpr](const QJsonValue& node, const QJsonValue& root) -> bool {
            qCDebug(jsonPathLog) << "Evaluating absolute path comparison:" << QString::fromStdString(leftPath) 
                                 << QString::fromStdString(op) << QString::fromStdString(rightExpr);
            
            // Evaluate left side (absolute path) with context-aware optimization
            QJsonValue leftValue;
            if (leftPath == "$") {
                leftValue = root;
            } else {
                try {
                    auto absolutePath = json_query::JSONPath::create(QString::fromStdString(leftPath));
                    if (absolutePath) {
                        auto results = absolutePath->evaluateAll(root);
                        if (results && !results->isEmpty()) {
                            // Use ContextAwareContainerCursor for efficient result processing
                            auto cursor = internal::makeSimpleContextCursor(*results, root, node);
                            
                            // Get first result using zero-copy iteration
                            for (const auto& [result, ctx] : cursor) {
                                leftValue = result;
                                break; // Take first result
                            }
                            
                            // Fallback if cursor iteration fails
                            if (leftValue.isUndefined()) {
                                leftValue = results->first();
                            }
                        }
                    }
                } catch (...) {
                    qCDebug(jsonPathLog) << "Failed to evaluate absolute path:" << QString::fromStdString(leftPath);
                    return false;
                }
            }
            
            // Evaluate right side with context-aware optimization
            QJsonValue rightValue;
            QString rightExprStr = QString::fromStdString(rightExpr);
            if (rightExprStr == "$") {
                rightValue = root;
            } else if (rightExprStr.startsWith("@")) {
                // Right side is a relative path - evaluate against current node
                if (rightExprStr == "@") {
                    rightValue = node;
                } else {
                    // Handle @.property, @[index], etc. with context-aware access
                    QString relativePath = rightExprStr.mid(1); // Remove @
                    if (relativePath.startsWith(".")) {
                        // Property access like @.value - use context-aware cursor for efficient access
                        QString propName = relativePath.mid(1);
                        if (node.isObject()) {
                            // Use context-aware cursor for efficient property lookup
                            auto cursor = makeSimpleContextCursor(node.toObject(), root, node);
                            
                            // Traditional property access (cursor doesn't expose keys in current interface)
                            rightValue = node.toObject().value(propName);
                        }
                    }
                    // Add more relative path handling as needed
                }
            } else {
                // Right side might be a literal value
                // Try to parse as JSON literal
                if (rightExprStr == "null") {
                    rightValue = QJsonValue::Null;
                } else if (rightExprStr == "true") {
                    rightValue = QJsonValue(true);
                } else if (rightExprStr == "false") {
                    rightValue = QJsonValue(false);
                } else {
                    // Try as number or string
                    bool ok;
                    double num = rightExprStr.toDouble(&ok);
                    if (ok) {
                        rightValue = QJsonValue(num);
                    } else {
                        // Treat as string (remove quotes if present)
                        if ((rightExprStr.startsWith('"') && rightExprStr.endsWith('"')) ||
                            (rightExprStr.startsWith('\'') && rightExprStr.endsWith('\''))) {
                            rightValue = QJsonValue(rightExprStr.mid(1, rightExprStr.length() - 2));
                        } else {
                            rightValue = QJsonValue(rightExprStr);
                        }
                    }
                }
            }
            
            // Perform comparison
            QString opStr = QString::fromStdString(op);
            if (opStr == "==") {
                if (leftValue.isUndefined() && rightValue.isUndefined()) return true;
                if (leftValue.isUndefined() && rightValue.toDouble() == 0) return true;
                if (rightValue.isUndefined() && leftValue.toDouble() == 0) return true;
                if (leftValue.isUndefined() && !rightValue.isUndefined()) return true;
                if (rightValue.isUndefined() && !leftValue.isUndefined()) return false; // Asymmetric
                return leftValue == rightValue;
            }
            if (opStr == "!=") {
                if (leftValue.isUndefined() && rightValue.isUndefined()) return false;
                if (leftValue.isUndefined() && rightValue.toDouble() == 0) return false;
                if (rightValue.isUndefined() && leftValue.toDouble() == 0) return false;
                return leftValue != rightValue;
            }
            if (opStr == "<") {
                // Implement ordering comparison logic as needed
                if (leftValue.isDouble() && rightValue.isDouble()) {
                    return leftValue.toDouble() < rightValue.toDouble();
                }
                return false;
            }
            if (opStr == ">") {
                if (leftValue.isDouble() && rightValue.isDouble()) {
                    return leftValue.toDouble() > rightValue.toDouble();
                }
                return false;
            }
            if (opStr == "<=") {
                if (leftValue.isDouble() && rightValue.isDouble()) {
                    return leftValue.toDouble() <= rightValue.toDouble();
                }
                return false;
            }
            if (opStr == ">=") {
                if (leftValue.isDouble() && rightValue.isDouble()) {
                    return leftValue.toDouble() >= rightValue.toDouble();
                }
                return false;
            }
            
            return false;
        });
    }
    
    // Handle function call patterns like length(@.a) == value($..c)
    if (s.contains("length(") || s.contains("value($")) {
        qCDebug(jsonPathLog) << "parseAbsolutePathContext: detected function call pattern:" << s;
        
        // Simple pattern matching for function calls using string operations
        // Look for comparison operators in order of precedence (longest first)
        const QStringList operators = {"<=", ">=", "==", "!=", "<", ">"};
        QString op;
        int opPos = -1;
        
        for (const QString& testOp : operators) {
            int pos = s.indexOf(testOp);
            if (pos != -1) {
                op = testOp;
                opPos = pos;
                break;
            }
        }
        
        if (opPos != -1) {
            QString left = s.left(opPos).trimmed();
            QString right = s.mid(opPos + op.length()).trimmed();
            
            qCDebug(jsonPathLog) << "parseAbsolutePathContext: function call parts - left:" << left << "op:" << op << "right:" << right;
            
            // Check if we need root context (value($...))
            bool needsRootContext = left.contains("value($") || right.contains("value($");
            
            if (needsRootContext) {
                qCDebug(jsonPathLog) << "parseAbsolutePathContext: creating context-aware function filter";
                
                struct ContextBuilder {
                    QVector<ContextFilterFn>& fns;
                    
                    [[nodiscard]] Token add(ContextFilterFn fn, QString key = {})
                    {
                        fns.push_back(std::move(fn));
                        const std::size_t id = fns.size() - 1;
                        Token token{Token::Kind::Filter, 0, {}, 0u, std::move(key)};
                        token.contextFilterId = id;
                        return token;
                    }
                };
                
                ContextBuilder b{out};
                return b.add([left, op, right](const QJsonValue& node, const QJsonValue& root) -> bool {
                    qCDebug(jsonPathLog) << "Evaluating context function:" << left << op << right;
                    
                    QJsonValue leftVal, rightVal;
                    bool leftIsNothing = false, rightIsNothing = false;
                    
                    // Evaluate left side with context-aware optimization
                    if (left.contains("(")) {
                        leftVal = evaluateContextFunction(left, node, root);
                        // Check if this represents "nothing" (undefined/null property in length)
                        if (left.startsWith("length(@.")) {
                            QString prop = left.mid(9, left.length() - 10); // Extract property name
                            if (node.isObject()) {
                                // Use context-aware cursor for efficient property existence check
                                auto cursor = internal::makeSimpleContextCursor(node.toObject(), root, node);
                                
                                // Check property existence and null values using traditional access
                                // (cursor doesn't expose keys in current interface)
                                QJsonValue propValue = node.toObject().value(prop);
                                if (propValue.isUndefined() || propValue.isNull()) {
                                    leftIsNothing = true;
                                }
                            }
                        }
                    } else if (left.startsWith("@.")) {
                        QString prop = left.mid(2);
                        if (node.isObject()) {
                            // Use context-aware cursor for efficient property access
                            auto cursor = makeSimpleContextCursor(node.toObject(), root, node);
                            
                            // Traditional property access (cursor doesn't expose keys)
                            QJsonValue val = node.toObject().value(prop);
                            if (val.isUndefined()) {
                                leftIsNothing = true;
                                leftVal = QJsonValue(0);
                            } else {
                                leftVal = val;
                            }
                        }
                    } else {
                        leftVal = QJsonValue(left);
                    }
                    
                    // Evaluate right side with context-aware optimization
                    if (right.contains("(")) {
                        rightVal = evaluateContextFunction(right, node, root);
                        // Check if this represents "nothing" (empty results in value)
                        if (right.startsWith("value($")) {
                            using json_query::JSONPath;
                            QString pathStr = right.mid(6, right.length() - 7); // Extract path
                            auto path = JSONPath::create(pathStr);
                            if (path) {
                                auto results = path->evaluateAll(root);
                                if (results) {
                                    if (results->isEmpty()) {
                                        rightIsNothing = true;
                                    } else {
                                        // Use context-aware cursor for efficient result validation
                                        auto cursor = makeSimpleContextCursor(*results, root, node);
                                        
                                        // Check if results are effectively empty using zero-copy iteration
                                        bool hasValidResults = false;
                                        for (const auto& [result, ctx] : cursor) {
                                            if (!result.isUndefined() && !result.isNull()) {
                                                hasValidResults = true;
                                                break;
                                            }
                                        }
                                        
                                        if (!hasValidResults) {
                                            rightIsNothing = true;
                                        }
                                    }
                                }
                            }
                        }
                    } else if (right.startsWith("@.")) {
                        QString prop = right.mid(2);
                        if (node.isObject()) {
                            // Use context-aware cursor for efficient property access
                            auto cursor = makeSimpleContextCursor(node.toObject(), root, node);
                            
                            // Traditional property access (cursor doesn't expose keys)
                            QJsonValue val = node.toObject().value(prop);
                            if (val.isUndefined()) {
                                rightIsNothing = true;
                                rightVal = QJsonValue(0);
                            } else {
                                rightVal = val;
                            }
                        }
                    } else {
                        rightVal = QJsonValue(right);
                    }
                    
                    qCDebug(jsonPathLog) << "Function comparison values:" << leftVal << op << rightVal 
                                         << "leftIsNothing:" << leftIsNothing << "rightIsNothing:" << rightIsNothing;
                    
                    // RFC 9535 "nothing" semantics: nothing equals nothing or zero-length values
                    if (op == "==") {
                        if (leftIsNothing && rightIsNothing) return true;
                        if (leftIsNothing && rightVal.toDouble() == 0) return true;
                        if (rightIsNothing && leftVal.toDouble() == 0) return true;
                        if (leftIsNothing && !rightIsNothing) return true;
                        if (rightIsNothing && !leftIsNothing) return false; // Asymmetric
                        // Both sides have actual values - use normal comparison
                        return leftVal == rightVal;
                    }
                    if (op == "!=") {
                        if (leftIsNothing && rightIsNothing) return false;
                        if (leftIsNothing && rightVal.toDouble() == 0) return false;
                        if (rightIsNothing && leftVal.toDouble() == 0) return false;
                        return leftVal != rightVal;
                    }
                    return false;
                }, s);
            }
        }
    }
    
    // Check for absolute path existence filters (allow wildcards and complex paths)
    // Valid: $, $.foo, $==@.value, $==$, etc.
    
    // Check for absolute path existence filters (allow wildcards and complex paths)
    // Valid: $, $.foo, $==@.value, $==$, etc.
    
    // Check for absolute path existence filters (allow wildcards and complex paths)
    // Valid: $, $.foo, $==@.value, $==$, etc.
    
    static const auto absExistencePat = ctre::match<R"(\$(\.[a-zA-Z_*][a-zA-Z0-9_]*|\[\d+\]|\[.*\])*)">;
    if (absExistencePat(s.toStdString())) {
        qCDebug(jsonPathLog) << "parseAbsolutePathContext: matched absolute path existence filter:" << s;
        
        struct ContextBuilder {
            QVector<ContextFilterFn>& fns;
            
            [[nodiscard]] Token add(ContextFilterFn fn, QString key = {})
            {
                fns.push_back(std::move(fn));
                const std::size_t id = fns.size() - 1;
                Token token{Token::Kind::Filter, 0, {}, 0u, std::move(key)};
                token.contextFilterId = id;
                return token;
            }
        };
        
        ContextBuilder b{out};
        return b.add([s](const QJsonValue& node, const QJsonValue& root) -> bool {
            qCDebug(jsonPathLog) << "Evaluating absolute path existence filter:" << s << "on root:" << root;
            
            // Basic implementation for absolute path existence filters
            if (s == "$") {
                // Root existence filter: always true if root exists
                return !root.isUndefined();
            }
            
            // Handle absolute path patterns
            if (s.startsWith("$.")) {
                // For patterns like "$.a", "$.*.a", etc., try to evaluate against root
                try {
                    // Create a temporary JSONPath to evaluate the absolute path
                    auto absolutePath = json_query::JSONPath::create(s);
                    if (absolutePath) {
                        auto results = absolutePath->evaluateAll(root);
                        if (results && !results->isEmpty()) {
                            return true;
                        }
                    }
                } catch (...) {
                    qCDebug(jsonPathLog) << "Failed to evaluate absolute path:" << s;
                }
            }
            
            // Fallback for unimplemented patterns
            qCDebug(jsonPathLog) << "Absolute path pattern not yet fully implemented:" << s;
            return false;
        });
    }
    
    qCDebug(jsonPathLog) << "parseAbsolutePathContext: no match for" << s;
    return std::nullopt;
}

// Context-aware filter compilation dispatcher
std::optional<Token> compileContextFilter(const QString& expr, QVector<ContextFilterFn>& contextOut, QVector<FilterFn>& regularOut)
{
    qCDebug(jsonPathLog) << "compileContextFilter called with expr:" << expr;
    
    // First try to parse as context-aware filter (absolute path references)
    if (auto contextToken = parseAbsolutePathContext(expr, contextOut)) {
        qCDebug(jsonPathLog) << "compileContextFilter: compiled as context-aware filter";
        return contextToken;
    }
    
    // Fall back to regular filter compilation
    qCDebug(jsonPathLog) << "compileContextFilter: falling back to regular filter";
    if (auto regularToken = compileFilter(expr, regularOut)) {
        qCDebug(jsonPathLog) << "compileContextFilter: regular filter compiled successfully";
        // Return the regular token as-is - no need to wrap for backward compatibility
        // The evaluation logic will handle both regular and context-aware filters appropriately
        return regularToken;
    }
    
    qCDebug(jsonPathLog) << "compileContextFilter: failed to compile filter";
    return std::nullopt;
}

} // namespace json_query::json_path
