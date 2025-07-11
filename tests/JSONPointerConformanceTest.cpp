// JSONPointerConformanceTest.cpp
// Conformance tests derived from RFC 6901 Appendix A examples

#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "json-query/JSONPointer.hpp"

class JSONPointerConformanceTest : public QObject
{
    Q_OBJECT

private:
    static const QJsonDocument &testDoc()
    {
        static const QJsonDocument doc([]{
            QJsonObject obj{
                {"foo", QJsonArray{QStringLiteral("bar"), QStringLiteral("baz")}},
                {"", 0},
                {"a/b", 1},
                {"c%d", 2},
                {"e^f", 3},
                {"g|h", 4},
                {"i\\j", 5},
                {"k\"l", 6},
                {" ", 7},
                {"m~n", 8}
            };
            return QJsonDocument(obj);
        }());
        return doc;
    }

private slots:
    void validPointers_data();
    void validPointers();

    void invalidPointers_data();
    void invalidPointers();
};

void JSONPointerConformanceTest::validPointers_data()
{
    QTest::addColumn<QString>("pointer");
    QTest::addColumn<QJsonValue>("expected");

    const QJsonObject root = testDoc().object();

    QTest::newRow("whole-doc") << QString() << QJsonValue(root);
    QTest::newRow("/foo")      << QString("/foo")      << root.value("foo");
    QTest::newRow("/foo/0")    << QString("/foo/0")    << root.value("foo").toArray().at(0);
    QTest::newRow("/")         << QString("/")         << root.value("");
    QTest::newRow("/a~1b")     << QString("/a~1b")     << root.value("a/b");
    QTest::newRow("/c%d")      << QString("/c%d")      << root.value("c%d");
    QTest::newRow("/e^f")      << QString("/e^f")      << root.value("e^f");
    QTest::newRow("/g|h")      << QString("/g|h")      << root.value("g|h");
    QTest::newRow("/i\\j")    << QString("/i\\j")    << root.value("i\\j");
    QTest::newRow("/k\"l")    << QString("/k\"l")    << root.value("k\"l");
    QTest::newRow("/ ")        << QString("/ ")        << root.value(" ");
    QTest::newRow("/m~0n")     << QString("/m~0n")     << root.value("m~n");
}

void JSONPointerConformanceTest::validPointers()
{
    QFETCH(QString, pointer);
    QFETCH(QJsonValue, expected);

    JSONPointer jp(pointer);
    QVERIFY2(jp.isValid(), qPrintable(QStringLiteral("Pointer should be valid: %1").arg(pointer)));

    QJsonValue actual = jp.evaluate(testDoc());
    if (expected.isObject()) {
        QCOMPARE(QJsonDocument(actual.toObject()).toJson(QJsonDocument::Compact),
                 QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact));
    } else if (expected.isArray()) {
        QCOMPARE(QJsonDocument(actual.toArray()).toJson(QJsonDocument::Compact),
                 QJsonDocument(expected.toArray()).toJson(QJsonDocument::Compact));
    } else {
        QCOMPARE(actual, expected);
    }
}

void JSONPointerConformanceTest::invalidPointers_data()
{
    QTest::addColumn<QString>("pointer");

    QTest::newRow("no-leading-slash") << QStringLiteral("foo/bar");
    QTest::newRow("double-slash")    << QStringLiteral("//");
    QTest::newRow("empty-token-mid") << QStringLiteral("/foo//bar");
}

void JSONPointerConformanceTest::invalidPointers()
{
    QFETCH(QString, pointer);

    JSONPointer jp(pointer);
    QVERIFY2(!jp.isValid(), qPrintable(QStringLiteral("Pointer should be invalid: %1").arg(pointer)));

    QJsonValue res = jp.evaluate(testDoc());
    QVERIFY(res.isUndefined());
}

#include "JSONPointerConformanceTest.moc"

QTEST_MAIN(JSONPointerConformanceTest)
