#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include <ctre.hpp>

namespace json_query::json_path {

// Context-aware filter parsing for absolute path references
std::optional<Token> parseAbsolutePathContext(QString s, QVector<ContextFilterFn>& out)
{
    qCDebug(jsonPathLog) << "parseAbsolutePathContext called with:" << s;
    
    // Pattern for absolute path references starting with $
    static const auto absolutePathPat = ctre::match<R"(\$.*)">;
    
    if (absolutePathPat(s.toStdString())) {
        qCDebug(jsonPathLog) << "parseAbsolutePathContext: matched absolute path pattern for:" << s;
        
        // Create a context-aware filter that uses the root document
        // For now, implement basic absolute path evaluation
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
            qCDebug(jsonPathLog) << "Evaluating absolute path filter:" << s << "on root:" << root;
            
            // Basic implementation for simple absolute path references
            if (s == "$") {
                // Root existence filter: always true if root exists
                return !root.isUndefined();
            }
            
            // Handle simple absolute path patterns
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
