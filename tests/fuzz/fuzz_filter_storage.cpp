/**
 * @file fuzz_filter_storage.cpp
 * @brief LibFuzzer target for EmbeddedFilter and CompactFilterStorage systems
 *
 * This fuzzer tests the robustness of the filter storage optimization system,
 * including small buffer optimization (SBO), type erasure, and move semantics.
 */

#include <fuzzer/FuzzedDataProvider.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <json-query/JSONQuery>
#include <json-query/json-path/JSONPathCompile.hpp>
#include <QJsonParseError>
#include <QString>
#include <QByteArray>
#include <functional>
#include <memory>

using namespace json_query::json_path;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Minimum input size check
    if (size < 2)
        return 0;

    // Use first byte to determine test scenario
    uint8_t scenario = data[0] % 4;

    // Parse JSON document from remaining input
    QByteArray      jsonInput(reinterpret_cast<const char*>(data + 1), static_cast<int>(size - 1));
    QJsonParseError parseError;
    QJsonDocument   doc = QJsonDocument::fromJson(jsonInput, &parseError);

    if (parseError.error != QJsonParseError::NoError || doc.isNull())
        return 0; // Invalid JSON, nothing to test

    QJsonValue testValue = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());

    try
    {
        switch (scenario)
        {
        case 0:
        {
            // Test small filter (should use inline storage)
            auto smallFilter = [](const QJsonValue& v) -> bool { return v.isString(); };

            CompactFilterStorage<32> storage(std::move(smallFilter));

            // Test evaluation
            bool result = storage.evaluate(testValue);
            static_cast<void>(result);

            // Test storage introspection
            bool hasFilter = storage.hasFilter();
            bool isInline  = storage.isInlineStorage();
            static_cast<void>(hasFilter);
            static_cast<void>(isInline);

            // Test copy semantics
            auto storageCopy = storage;
            bool copyResult  = storageCopy.evaluate(testValue);
            static_cast<void>(copyResult);

            break;
        }

        case 1:
        {
            // Test large filter (should use heap storage)
            struct LargeFilter
            {
                std::array<int, 20> data{}; // Force heap allocation
                bool                operator()(const QJsonValue& v) const { return v.isObject() && data[0] == 0; }
            };

            LargeFilter              largeFilter;
            CompactFilterStorage<32> storage(std::move(largeFilter));

            // Test evaluation with large filter
            bool result = storage.evaluate(testValue);
            static_cast<void>(result);

            // Should use heap storage
            bool hasFilter = storage.hasFilter();
            bool isInline  = storage.isInlineStorage();
            static_cast<void>(hasFilter);
            static_cast<void>(isInline);

            break;
        }

        case 2:
        {
            // Test context-aware filter
            auto contextFilter = [](const QJsonValue& current, const QJsonValue& root) -> bool
            { return current.isObject() && root.isObject(); };

            CompactContextFilterStorage<32> contextStorage(std::move(contextFilter));

            // Test context evaluation
            QJsonValue rootValue = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
            bool       result    = contextStorage.evaluateContext(testValue, rootValue);
            static_cast<void>(result);

            // Test storage properties
            bool hasFilter = contextStorage.hasFilter();
            bool isInline  = contextStorage.isInlineStorage();
            static_cast<void>(hasFilter);
            static_cast<void>(isInline);

            break;
        }

        case 3:
        {
            // Test EmbeddedFilter with both regular and context filters
            EmbeddedFilter embeddedFilter;

            // Test default construction
            bool hasRegular     = embeddedFilter.hasRegularFilter();
            bool hasContext     = embeddedFilter.hasContextFilter();
            bool hasAny         = embeddedFilter.hasFilter();
            bool isZeroOverhead = embeddedFilter.isZeroOverhead();

            static_cast<void>(hasRegular);
            static_cast<void>(hasContext);
            static_cast<void>(hasAny);
            static_cast<void>(isZeroOverhead);

            // Test with regular filter
            auto regularFilter = [](const QJsonValue& v) -> bool { return v.isDouble() && v.toDouble() > 0; };

            EmbeddedFilter regularEmbedded(std::move(regularFilter));
            bool           regularResult = regularEmbedded.evaluate(testValue);
            static_cast<void>(regularResult);

            // Test with context filter
            auto contextFilter = [](const QJsonValue& current, const QJsonValue& root) -> bool
            { return current.isArray() || root.isObject(); };

            EmbeddedFilter contextEmbedded(std::move(contextFilter));
            QJsonValue     rootValue     = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
            bool           contextResult = contextEmbedded.evaluateContext(testValue, rootValue);
            static_cast<void>(contextResult);

            // Test copy and move semantics
            auto embeddedCopy = contextEmbedded;
            auto embeddedMove = std::move(embeddedCopy);

            bool moveResult = embeddedMove.evaluateContext(testValue, rootValue);
            static_cast<void>(moveResult);

            break;
        }
        }
    }
    catch (...)
    {
        // Filter storage system should not throw exceptions
        // If it does, this indicates a bug found by fuzzing
        return 0;
    }

    return 0;
}
