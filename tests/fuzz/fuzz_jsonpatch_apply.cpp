/**
 * @file fuzz_jsonpatch_apply.cpp
 * @brief LibFuzzer target for JSONPatch create + apply
 *
 * Fuzzes the patch document and the target document simultaneously and
 * asserts the apply invariants:
 *   - apply() never mutates its input (atomicity by construction)
 *   - applyInPlace() leaves the document untouched on failure
 *   - no input crashes create() or apply()
 */

#include <fuzzer/FuzzedDataProvider.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <json-query/JSONQuery>

using json_query::JSONPatch;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 4)
        return 0;

    FuzzedDataProvider provider(data, size);
    const std::string  patchStr{provider.ConsumeRandomLengthString(1024)};
    const std::string  docStr{provider.ConsumeRemainingBytesAsString()};

    QJsonParseError perr;
    const auto      patchDoc{QJsonDocument::fromJson(QByteArray::fromStdString(patchStr), &perr)};
    if (perr.error != QJsonParseError::NoError || !patchDoc.isArray())
        return 0;

    auto patch{JSONPatch::create(patchDoc.array())};
    if (!patch)
        return 0;

    QJsonDocument doc{QJsonDocument::fromJson(QByteArray::fromStdString(docStr), &perr)};
    if (perr.error != QJsonParseError::NoError || doc.isNull())
        doc = QJsonDocument(QJsonObject{{"seed", QJsonArray{1, 2, 3}}});

    const QJsonDocument before{doc};

    // Functional apply must not mutate its input
    auto applied{patch->apply(doc)};
    if (doc != before)
        __builtin_trap();

    // applyInPlace must be all-or-nothing and agree with apply()
    QJsonDocument inPlaceTarget{doc};
    const bool    ok{patch->applyInPlace(inPlaceTarget).has_value()};
    if (!ok && inPlaceTarget != before)
        __builtin_trap();
    if (ok != applied.has_value())
        __builtin_trap();

    // The QJsonValue overload must not crash either
    const QJsonValue root{doc.isArray() ? QJsonValue{doc.array()} : QJsonValue{doc.object()}};
    static_cast<void>(patch->apply(root));

    return 0;
}
