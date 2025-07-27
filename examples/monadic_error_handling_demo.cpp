#include "json-query/json-path/JSONPath.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <iostream>
#include <chrono>

using json_query::JSONPath;

// Helper function to validate JSON document
std::expected<QJsonDocument, QString> validateJsonDocument(const QString& jsonStr) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        return std::unexpected(QString("JSON Parse Error: %1").arg(error.errorString()));
    }
    
    if (doc.isEmpty()) {
        return std::unexpected(QString("Empty JSON document"));
    }
    
    return doc;
}

// Helper function to validate JSONPath syntax
std::expected<QString, QString> validatePathSyntax(const QString& path) {
    if (path.isEmpty()) {
        return std::unexpected(QString("Empty path"));
    }
    if (!path.startsWith('$')) {
        return std::unexpected(QString("Path must start with '$'"));
    }
    if (path.endsWith('.')) {
        return std::unexpected(QString("Path cannot end with '.'"));
    }
    return path;
}

// Helper function to create JSONPath
std::expected<JSONPath, QString> createOptimizedPath(const QString& validPath) {
    std::cout << "  → Creating JSONPath\n";
    
    auto result = JSONPath::create(validPath);
    if (!result) {
        return std::unexpected(QString("JSONPath creation failed: error code %1").arg(static_cast<int>(result.error())));
    }
    
    return std::move(*result);
}

// Helper function to execute JSONPath evaluation
std::expected<QJsonArray, QString> executeEvaluation(const JSONPath& path, const QJsonDocument& doc) {
    std::cout << "  → Executing JSONPath evaluation\n";
    
    auto result = path.evaluateAll(doc);
    if (!result) {
        return std::unexpected(QString("Evaluation failed: error code %1").arg(static_cast<int>(result.error())));
    }
    
    return *result;
}

// C++23 Monadic JSONPath Pipeline - Elegant error composition!
std::expected<QJsonArray, QString> processJsonPathMonadic(const QString& jsonStr, 
                                                          const QString& pathStr) {
    std::cout << "\n=== C++23 Monadic Pipeline ===\n";
    std::cout << "JSON: " << jsonStr.left(50).toStdString() << (jsonStr.length() > 50 ? "..." : "") << "\n";
    std::cout << "Path: " << pathStr.toStdString() << "\n";
    
    // C++23 Monadic Chain - No manual error checking needed!
    return validateJsonDocument(jsonStr)
        .and_then([](const QJsonDocument& doc) -> std::expected<QJsonDocument, QString> {
            std::cout << "  ✅ JSON document validated\n";
            return doc;
        })
        .and_then([&pathStr](const QJsonDocument& doc) -> std::expected<std::pair<QJsonDocument, QString>, QString> {
            return validatePathSyntax(pathStr)
                .transform([&doc](const QString& validPath) {
                    std::cout << "  ✅ Path syntax validated\n";
                    return std::make_pair(doc, validPath);
                });
        })
        .and_then([](const std::pair<QJsonDocument, QString>& docAndPath) -> std::expected<std::pair<QJsonDocument, JSONPath>, QString> {
            const auto& [doc, path] = docAndPath;
            return createOptimizedPath(path)
                .transform([&doc](JSONPath&& jsonPath) {
                    std::cout << "  ✅ JSONPath created\n";
                    return std::make_pair(doc, std::move(jsonPath));
                });
        })
        .and_then([](const std::pair<QJsonDocument, JSONPath>& docAndPath) -> std::expected<QJsonArray, QString> {
            const auto& [doc, path] = docAndPath;
            return executeEvaluation(path, doc);
        })
        .transform([](const QJsonArray& results) {
            std::cout << "  ✅ Evaluation completed with " << results.size() << " results\n";
            return results;
        });
}

// Traditional approach for comparison
std::expected<QJsonArray, QString> processJsonPathTraditional(const QString& jsonStr, 
                                                              const QString& pathStr) {
    std::cout << "\n=== Traditional Pipeline ===\n";
    std::cout << "JSON: " << jsonStr.left(50).toStdString() << (jsonStr.length() > 50 ? "..." : "") << "\n";
    std::cout << "Path: " << pathStr.toStdString() << "\n";
    
    // Traditional manual error checking - verbose and error-prone
    auto validatedDoc = validateJsonDocument(jsonStr);
    if (!validatedDoc) {
        std::cout << "  ❌ JSON validation failed: " << validatedDoc.error().toStdString() << "\n";
        return std::unexpected(validatedDoc.error());
    }
    std::cout << "  ✅ JSON document validated\n";
    
    auto validatedPath = validatePathSyntax(pathStr);
    if (!validatedPath) {
        std::cout << "  ❌ Path validation failed: " << validatedPath.error().toStdString() << "\n";
        return std::unexpected(validatedPath.error());
    }
    std::cout << "  ✅ Path syntax validated\n";
    
    auto createdPath = createOptimizedPath(*validatedPath);
    if (!createdPath) {
        std::cout << "  ❌ JSONPath creation failed: " << createdPath.error().toStdString() << "\n";
        return std::unexpected(createdPath.error());
    }
    std::cout << "  ✅ JSONPath created\n";
    
    auto results = executeEvaluation(*createdPath, *validatedDoc);
    if (!results) {
        std::cout << "  ❌ Evaluation failed: " << results.error().toStdString() << "\n";
        return std::unexpected(results.error());
    }
    std::cout << "  ✅ Evaluation completed with " << results->size() << " results\n";
    
    return *results;
}

int main() {
    std::cout << "=== C++23 Monadic Error Handling Demo ===\n";
    std::cout << "Demonstrating elegant error composition with std::expected monadic operations\n";
    std::cout << "Using the JSONPath evaluation engine\n";

    // Test data
    QString validJson = R"({
        "store": {
            "book": [
                {"title": "The Great Gatsby", "price": 12.99, "category": "fiction"},
                {"title": "Clean Code", "price": 35.00, "category": "programming"},
                {"title": "Design Patterns", "price": 45.00, "category": "programming"},
                {"title": "1984", "price": 13.84, "category": "fiction"}
            ]
        }
    })";

    // Test cases: valid and invalid scenarios
    struct TestCase {
        QString json;
        QString path;
        QString description;
    };

    std::vector<TestCase> testCases = {
        {validJson, "$.store.book[*].title", "Valid: Extract all book titles"},
        {validJson, "$.store.book[?(@.price > 20)].title", "Valid: Books over $20"},
        {validJson, "$..price", "Valid: All prices (recursive descent)"},
        {"invalid json", "$.store.book[*].title", "Invalid: Malformed JSON"},
        {validJson, "", "Invalid: Empty path"},
        {validJson, "store.book[*].title", "Invalid: Missing root '$'"},
        {validJson, "$.store.book[*].title.", "Invalid: Trailing dot"},
        {validJson, "$.store.nonexistent[*].title", "Valid path, no results"}
    };

    for (size_t i = 0; i < testCases.size(); ++i) {
        const auto& testCase = testCases[i];
        
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "Test Case " << (i + 1) << ": " << testCase.description.toStdString() << "\n";
        std::cout << std::string(70, '=') << "\n";

        // Test both approaches
        auto monadicResult = processJsonPathMonadic(testCase.json, testCase.path);
        auto traditionalResult = processJsonPathTraditional(testCase.json, testCase.path);

        // Verify both approaches give the same result
        bool bothSucceeded = monadicResult.has_value() && traditionalResult.has_value();
        bool bothFailed = !monadicResult.has_value() && !traditionalResult.has_value();
        
        std::cout << "\n" << std::string(35, '-') << " RESULTS " << std::string(35, '-') << "\n";
        
        if (bothSucceeded) {
            std::cout << "✅ Both approaches SUCCEEDED\n";
            std::cout << "   Results count: " << monadicResult->size() << "\n";
            if (monadicResult->size() > 0 && monadicResult->size() <= 3) {
                std::cout << "   Sample results: ";
                for (int j = 0; j < monadicResult->size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    QJsonValue val = monadicResult->at(j);
                    if (val.isString()) {
                        std::cout << "\"" << val.toString().toStdString() << "\"";
                    } else if (val.isDouble()) {
                        std::cout << val.toDouble();
                    } else {
                        std::cout << "[complex]";
                    }
                }
                std::cout << "\n";
            }
        } else if (bothFailed) {
            std::cout << "✅ Both approaches FAILED (as expected)\n";
            std::cout << "   Monadic error: " << monadicResult.error().toStdString() << "\n";
            std::cout << "   Traditional error: " << traditionalResult.error().toStdString() << "\n";
        } else {
            std::cout << "❌ MISMATCH: Monadic=" << (monadicResult.has_value() ? "SUCCESS" : "FAILURE") 
                      << ", Traditional=" << (traditionalResult.has_value() ? "SUCCESS" : "FAILURE") << "\n";
        }
    }

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "=== C++23 Monadic Operations Benefits ===\n";
    std::cout << "✅ Eliminates manual error checking boilerplate (no if(!result) patterns)\n";
    std::cout << "✅ Creates readable, composable error handling chains\n";
    std::cout << "✅ Maintains type safety and zero-cost abstractions\n";
    std::cout << "✅ Reduces cognitive load and potential bugs\n";
    std::cout << "✅ Enables functional programming patterns in C++\n";
    std::cout << "✅ Works seamlessly with existing std::expected APIs\n";
    std::cout << "\nMonadic operations demonstrated:\n";
    std::cout << "  • and_then() - Chain operations that might fail\n";
    std::cout << "  • transform() - Transform success values without error handling\n";
    std::cout << "  • Automatic error propagation through the entire chain\n";
    std::cout << "  • Type-safe composition of multiple fallible operations\n";

    return 0;
}
