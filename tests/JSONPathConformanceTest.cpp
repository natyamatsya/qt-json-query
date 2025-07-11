// JSONPathConformanceTest.cpp
// Minimal conformance sanity-suite for the JSONPath implementation.
// The cases are sourced from the draft IETF JSONPath spec v0.9 and
// https://goessner.net/articles/JsonPath/ examples.
// Goal: ensure evaluation matches reference expectations; this test does
// *not* attempt to be exhaustive yet.

#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "json-query/JSONPath.hpp"

class JSONPathConformanceTest : public QObject
{
    Q_OBJECT

private:
    static const QJsonDocument &sampleDoc()
    {
        // Typical store example from Goessner article
        static const QJsonDocument doc([]{
            QJsonObject bicycle{{"color", "red"}, {"price", 19.95}};
            QJsonArray books{
                QJsonObject{{"category", "reference"}, {"author", "Nigel Rees"}, {"title", "Sayings of the Century"}, {"price", 8.95}},
                QJsonObject{{"category", "fiction"}, {"author", "Evelyn Waugh"}, {"title", "Sword of Honour"}, {"price", 12.99}},
                QJsonObject{{"category", "fiction"}, {"author", "Herman Melville"}, {"title", "Moby Dick"}, {"isbn", "0-553-21311-3"}, {"price", 8.99}},
                QJsonObject{{"category", "fiction"}, {"author", "J. R. R. Tolkien"}, {"title", "The Lord of the Rings"}, {"isbn", "0-395-19395-8"}, {"price", 22.99}}
            };
            QJsonObject store{{"book", books}, {"bicycle", bicycle}};
            QJsonObject root{{"store", store}, {"expensive", 10}};
            return QJsonDocument(root);
        }());
        return doc;
    }

private slots:
    void validPaths_data();
    void validPaths();
};

void JSONPathConformanceTest::validPaths_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QJsonValue>("expected");

    const auto root = sampleDoc().object();
    const auto books = root["store"].toObject()["book"].toArray();

    QTest::newRow("root") << QStringLiteral("$") << QJsonValue(root);
    QTest::newRow("dot-child") << QStringLiteral("$.store.book[0].author") << QJsonValue("Nigel Rees");
    QTest::newRow("bracket-child") << QStringLiteral("$['store']['bicycle']['color']") << QJsonValue("red");
    QTest::newRow("wildcard array index") << QStringLiteral("$.store.book[*].price") << QJsonValue::fromVariant(QVariantList{8.95,12.99,8.99,22.99});
    QTest::newRow("filter-price") << QStringLiteral("$.store.book[?(@.price > 20)].title") << QJsonValue::fromVariant(QVariantList{"The Lord of the Rings"});
    QTest::newRow("recursive search isbn") << QStringLiteral("$..isbn") << QJsonValue::fromVariant(QVariantList{"0-553-21311-3","0-395-19395-8"});
}

void JSONPathConformanceTest::validPaths()
{
    QFETCH(QString, path);
    QFETCH(QJsonValue, expected);

    JSONPath jp(path);
    QVERIFY2(jp.isValid(), qPrintable(QStringLiteral("Path should be valid: %1").arg(path)));

    const QJsonValue actual = jp.evaluate(sampleDoc());

    if (expected.isArray()) {
        QCOMPARE(QJsonDocument(actual.toArray()).toJson(QJsonDocument::Compact),
                 QJsonDocument(expected.toArray()).toJson(QJsonDocument::Compact));
    } else if (expected.isObject()) {
        QCOMPARE(QJsonDocument(actual.toObject()).toJson(QJsonDocument::Compact),
                 QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact));
    } else {
        QCOMPARE(actual, expected);
    }
}

#include "JSONPathConformanceTest.moc"

QTEST_MAIN(JSONPathConformanceTest)
