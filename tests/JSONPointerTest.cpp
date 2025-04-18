// jsonpointertest.cpp
#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "JSONPointer.h"

class JSONPointerTest : public QObject
{
    Q_OBJECT

private slots:
    void testEmptyPointer();
    void testSimpleObjectAccess();
    void testNestedObjectAccess();
    void testArrayAccess();
    void testMixedObjectArrayAccess();
    void testEscaping();
    void testInvalidPointers();
    void testNonExistentPath();
};

void JSONPointerTest::testEmptyPointer()
{
    QJsonObject obj{{"foo", "bar"}};
    QJsonDocument doc(obj);

    JSONPointer pointer("");
    QVERIFY(pointer.isValid());
    QCOMPARE(pointer.evaluate(doc).toObject(), obj);
}

void JSONPointerTest::testSimpleObjectAccess()
{
    QJsonObject obj{
        {"foo", "bar"},
        {"baz", 42}};
    QJsonDocument doc(obj);

    // Test accessing the "foo" property
    JSONPointer pointerFoo("/foo");
    QVERIFY(pointerFoo.isValid());
    QCOMPARE(pointerFoo.evaluate(doc).toString(), QString("bar"));

    // Test accessing the "baz" property
    JSONPointer pointerBaz("/baz");
    QVERIFY(pointerBaz.isValid());
    QCOMPARE(pointerBaz.evaluate(doc).toInt(), 42);
}

void JSONPointerTest::testNestedObjectAccess()
{
    QJsonObject innerObj{{"inner", "value"}};
    QJsonObject obj{
        {"nested", innerObj},
        {"foo", "bar"}};
    QJsonDocument doc(obj);

    // Test accessing the nested "inner" property
    JSONPointer pointer("/nested/inner");
    QVERIFY(pointer.isValid());
    QCOMPARE(pointer.evaluate(doc).toString(), QString("value"));
}

void JSONPointerTest::testArrayAccess()
{
    QJsonArray array = QJsonArray::fromStringList({"zero", "one", "two"});
    QJsonDocument doc(array);

    // Test accessing array elements
    JSONPointer pointer0("/0");
    QVERIFY(pointer0.isValid());
    QCOMPARE(pointer0.evaluate(doc).toString(), QString("zero"));

    JSONPointer pointer1("/1");
    QVERIFY(pointer1.isValid());
    QCOMPARE(pointer1.evaluate(doc).toString(), QString("one"));

    JSONPointer pointer2("/2");
    QVERIFY(pointer2.isValid());
    QCOMPARE(pointer2.evaluate(doc).toString(), QString("two"));
}

void JSONPointerTest::testMixedObjectArrayAccess()
{
    QJsonArray innerArray = QJsonArray::fromStringList({"a", "b", "c"});
    QJsonObject obj{
        {"array", innerArray},
        {"foo", "bar"}};
    QJsonDocument doc(obj);

    // Test accessing an element in a nested array
    JSONPointer pointer("/array/1");
    QVERIFY(pointer.isValid());
    QCOMPARE(pointer.evaluate(doc).toString(), QString("b"));
}

void JSONPointerTest::testEscaping()
{
    QJsonObject obj{
        {"foo/bar", "value"},
        {"foo~bar", "other value"}};
    QJsonDocument doc(obj);

    // Test accessing a property with '/' in its name
    JSONPointer pointerSlash("/foo~1bar");
    QVERIFY(pointerSlash.isValid());
    QCOMPARE(pointerSlash.evaluate(doc).toString(), QString("value"));

    // Test accessing a property with '~' in its name
    JSONPointer pointerTilde("/foo~0bar");
    QVERIFY(pointerTilde.isValid());
    QCOMPARE(pointerTilde.evaluate(doc).toString(), QString("other value"));
}

void JSONPointerTest::testInvalidPointers()
{
    // Test pointer without leading '/'
    JSONPointer invalidPointer("foo/bar");
    QVERIFY(!invalidPointer.isValid());

    QJsonObject obj{{"foo", "bar"}};
    QJsonDocument doc(obj);

    // Evaluation of invalid pointer should return undefined
    QVERIFY(invalidPointer.evaluate(doc).isUndefined());
}

void JSONPointerTest::testNonExistentPath()
{
    QJsonObject obj{{"foo", "bar"}};
    QJsonDocument doc(obj);

    // Test accessing a non-existent property
    JSONPointer pointer("/nonexistent");
    QVERIFY(pointer.isValid());
    QVERIFY(pointer.evaluate(doc).isUndefined());

    // Test accessing a property as array
    JSONPointer pointerArrayAccess("/foo/0");
    QVERIFY(pointerArrayAccess.isValid());
    QVERIFY(pointerArrayAccess.evaluate(doc).isUndefined());
}

QTEST_MAIN(JSONPointerTest)
