// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// Demonstrates document mutation: JSONPointer write operations
// (add/replace/remove/set, RFC 6902 §4 semantics), JSONPatch (RFC 6902,
// atomic apply), and merge_patch (RFC 7386).

#include "json-query/JSONQuery"
#include "json-query/json-patch/JSONMergePatch.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using json_query::JSONPatch;
using json_query::JSONPointer;

static void dump(QStringView label, const QJsonDocument& doc)
{
    qDebug().noquote() << label << doc.toJson(QJsonDocument::Compact);
}

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};

    qDebug() << "=== JSON Pointer writes (RFC 6901 + RFC 6902 section 4 semantics) ===";

    // The settings-backend shape: build up a document from nothing with
    // set(createIntermediates) — missing objects/arrays appear on the way.
    QJsonDocument settings;
    const auto set = [&settings](QStringView path, const QJsonValue& value)
    {
        auto ptr{JSONPointer::create(path)};
        if (auto r{ptr->set(settings, value, {.createIntermediates = true})}; !r)
            qWarning().noquote() << "  set" << path << "failed:" << r.error().formatted_message();
    };
    set(u"/ui/theme/accent", "teal");
    set(u"/ui/theme/dark", true);
    set(u"/recent/0", "a.json"); // next token 0 → creates an array
    set(u"/recent/-", "b.json"); // "-" appends
    dump(u"settings:", settings);

    // RFC 6902 primitives: add / replace / remove (strong guarantee — a
    // failing write never leaves a half-applied document)
    auto accent{JSONPointer::create(u"/ui/theme/accent").value()};
    if (auto r{accent.replace(settings, "crimson")})
        dump(u"after replace:", settings);

    if (auto removed{JSONPointer::create(u"/recent/0").value().remove(settings)})
    {
        qDebug() << "removed value:" << *removed;
        dump(u"after remove:", settings);
    }

    // Errors carry the failing token index; the document stays untouched
    auto bad{JSONPointer::create(u"/no/such/path").value().replace(settings, 1)};
    if (!bad)
        qDebug().noquote() << "expected failure:" << bad.error().formatted_message();

    qDebug() << "\n=== JSON Patch (RFC 6902) — atomic multi-op ===";

    const QJsonArray patchJson{
        QJsonObject{{"op", "test"}, {"path", "/ui/theme/accent"}, {"value", "crimson"}},
        QJsonObject{{"op", "replace"}, {"path", "/ui/theme/accent"}, {"value", "ocean"}},
        QJsonObject{{"op", "copy"}, {"from", "/ui/theme"}, {"path", "/ui/backupTheme"}},
        QJsonObject{{"op", "add"}, {"path", "/recent/-"}, {"value", "c.json"}},
    };
    auto patch{JSONPatch::create(patchJson)};
    if (!patch)
    {
        qWarning().noquote() << "patch invalid:" << patch.error().formatted_message();
        return EXIT_FAILURE;
    }
    if (auto r{patch->applyInPlace(settings)})
        dump(u"after patch:", settings);
    else // all-or-nothing: settings would be untouched here
        qWarning().noquote() << "patch failed:" << r.error().formatted_message();

    qDebug() << "\n=== JSON Merge Patch (RFC 7386) — fragment-style update ===";

    const QJsonDocument fragment(QJsonObject{
        {"ui", QJsonObject{{"backupTheme", QJsonValue::Null}, // null removes
                           {"theme", QJsonObject{{"dark", false}}}}}, // merges
    });
    settings = json_query::json_patch::merge_patch(settings, fragment);
    dump(u"after merge patch:", settings);

    return EXIT_SUCCESS;
}
