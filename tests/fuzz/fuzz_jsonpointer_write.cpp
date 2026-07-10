/**
 * @file fuzz_jsonpointer_write.cpp
 * @brief LibFuzzer target for JSONPointer write operations (add/replace/remove/set)
 *
 * Fuzzes the pointer expression, the target document, and the written value
 * simultaneously, and asserts the write invariants:
 *   - a successful add/set round-trips through evaluate()
 *   - a failed write leaves the document bit-identical (strong guarantee)
 *   - no input crashes the walk
 */

#include <fuzzer/FuzzedDataProvider.h>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>
#include <json-query/JSONQuery>

using json_query::json_pointer::JSONPointer;
using json_query::json_pointer::WriteOptions;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 4)
        return 0;

    FuzzedDataProvider provider(data, size);

    const auto opSelector{provider.ConsumeIntegralInRange<uint8_t>(0, 3)};
    const bool createIntermediates{provider.ConsumeBool()};

    const std::string pointerStr{provider.ConsumeRandomLengthString(256)};
    const std::string valueStr{provider.ConsumeRandomLengthString(256)};
    const std::string docStr{provider.ConsumeRemainingBytesAsString()};

    auto pointer{JSONPointer::create(QString::fromUtf8(pointerStr.data(), pointerStr.size()))};
    if (!pointer)
        return 0;

    QJsonParseError perr;
    QJsonDocument   doc{QJsonDocument::fromJson(QByteArray::fromStdString(docStr), &perr)};
    if (perr.error != QJsonParseError::NoError || doc.isNull())
        doc = QJsonDocument(QJsonObject{{"seed", QJsonArray{1, 2, 3}}});

    // Value: parsed JSON if possible, raw string otherwise
    QJsonValue value{QString::fromUtf8(valueStr.data(), valueStr.size())};
    if (const auto valueDoc{QJsonDocument::fromJson(QByteArray::fromStdString(valueStr), &perr)};
        perr.error == QJsonParseError::NoError && !valueDoc.isNull())
        value = valueDoc.isObject() ? QJsonValue{valueDoc.object()} : QJsonValue{valueDoc.array()};

    const QJsonDocument before{doc};

    bool ok{false};
    bool wasAdd{false};
    switch (opSelector)
    {
    case 0:
        ok     = pointer->add(doc, value).has_value();
        wasAdd = true;
        break;
    case 1:
        ok = pointer->replace(doc, value).has_value();
        break;
    case 2:
        ok = pointer->remove(doc).has_value();
        break;
    default:
        ok     = pointer->set(doc, value, WriteOptions{.createIntermediates = createIntermediates}).has_value();
        wasAdd = true;
        break;
    }

    if (!ok)
    {
        // Strong guarantee: a failed write leaves the document untouched
        if (doc != before)
            __builtin_trap();
    }
    else if (wasAdd)
    {
        // A successful add/set must round-trip through evaluate() — except for
        // an array-append "-" leaf, whose written position is not readable via
        // the same pointer (RFC 6902 §4.1).
        auto readBack{pointer->evaluate(doc)};
        if (readBack.has_value())
        {
            const QJsonValue normalized{value.isUndefined() ? QJsonValue{} : value};
            if (*readBack != normalized)
                __builtin_trap();
        }
    }

    // The value-overload walk must never crash either
    QJsonValue root{doc.isArray() ? QJsonValue{doc.array()} : QJsonValue{doc.object()}};
    static_cast<void>(pointer->set(root, value, WriteOptions{.createIntermediates = createIntermediates}));

    return 0;
}
