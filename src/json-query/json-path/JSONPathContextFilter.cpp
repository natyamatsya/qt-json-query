#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include <ctre.hpp>

namespace json_query::json_path {

// Context-aware filter parsing for absolute path references
std::optional<Token> parseAbsolutePathContext(QString s, QVector<ContextFilterFn>& out)
{
    qCDebug(jsonPathLog) << "parseAbsolutePathContext called with:" << s;
    
    // RFC 9535 allows absolute path references in comparisons
    // Handle patterns like: $, $.foo, $==@.value, $==$, etc.
    
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
                Token token{Token::Kind::Filter, 0, {}, 0u, std::move(key), 0};
                token.contextFilterId = id;
                return token;
            }
        };
        
        ContextBuilder b{out};
        return b.add([leftPath, op, rightExpr](const QJsonValue& node, const QJsonValue& root) -> bool {
            qCDebug(jsonPathLog) << "Evaluating absolute path comparison:" << QString::fromStdString(leftPath) 
                                 << QString::fromStdString(op) << QString::fromStdString(rightExpr);
            
            // Evaluate left side (absolute path)
            QJsonValue leftValue;
            if (leftPath == "$") {
                leftValue = root;
            } else {
                try {
                    auto absolutePath = json_query::JSONPath::create(QString::fromStdString(leftPath));
                    if (absolutePath) {
                        auto results = absolutePath->evaluateAll(root);
                        if (!results.isEmpty()) {
                            leftValue = results.first();
                        }
                    }
                } catch (...) {
                    qCDebug(jsonPathLog) << "Failed to evaluate absolute path:" << QString::fromStdString(leftPath);
                    return false;
                }
            }
            
            // Evaluate right side
            QJsonValue rightValue;
            QString rightExprStr = QString::fromStdString(rightExpr);
            if (rightExprStr == "$") {
                rightValue = root;
            } else if (rightExprStr.startsWith("@")) {
                // Right side is a relative path - evaluate against current node
                if (rightExprStr == "@") {
                    rightValue = node;
                } else {
                    // Handle @.property, @[index], etc.
                    QString relativePath = rightExprStr.mid(1); // Remove @
                    if (relativePath.startsWith(".")) {
                        // Property access like @.value
                        QString propName = relativePath.mid(1);
                        if (node.isObject()) {
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
                return leftValue == rightValue;
            } else if (opStr == "!=") {
                return leftValue != rightValue;
            } else if (opStr == "<") {
                // Implement ordering comparison logic as needed
                if (leftValue.isDouble() && rightValue.isDouble()) {
                    return leftValue.toDouble() < rightValue.toDouble();
                }
                return false;
            } else if (opStr == ">") {
                if (leftValue.isDouble() && rightValue.isDouble()) {
                    return leftValue.toDouble() > rightValue.toDouble();
                }
                return false;
            } else if (opStr == "<=") {
                if (leftValue.isDouble() && rightValue.isDouble()) {
                    return leftValue.toDouble() <= rightValue.toDouble();
                }
                return false;
            } else if (opStr == ">=") {
                if (leftValue.isDouble() && rightValue.isDouble()) {
                    return leftValue.toDouble() >= rightValue.toDouble();
                }
                return false;
            }
            
            return false;
        });
    }
    
    // Check for absolute path existence filters (allow wildcards and complex paths)
    // Valid: $.*.a, $.foo.bar, $.a[0].b, etc.
    static const auto absExistencePat = ctre::match<R"(\$(\.[a-zA-Z_*][a-zA-Z0-9_]*|\[\d+\]|\[.*\])*)">;
    if (absExistencePat(s.toStdString())) {
        qCDebug(jsonPathLog) << "parseAbsolutePathContext: matched absolute path existence filter:" << s;
        
        struct ContextBuilder {
            QVector<ContextFilterFn>& fns;
            
            [[nodiscard]] Token add(ContextFilterFn fn, QString key = {})
            {
                fns.push_back(std::move(fn));
                const std::size_t id = fns.size() - 1;
                Token token{Token::Kind::Filter, 0, {}, 0u, std::move(key), 0};
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
                        // Return true if the absolute path yields any results
                        bool hasResults = !results.isEmpty();
                        qCDebug(jsonPathLog) << "Absolute path" << s << "evaluation result:" << hasResults << "(" << results.size() << "results)";
                        return hasResults;
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
