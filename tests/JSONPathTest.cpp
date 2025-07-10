// jsonpathtest.cpp
#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "json_query/JSONPath.h"
#include "json_query/JSONPointer.h"

class JSONPathTest : public QObject
{
    Q_OBJECT

private slots:
    void testRootAccess();
    void testSimplePropertyAccess();
    void testNestedPropertyAccess();
    void testArrayIndexAccess();
    void testArraySliceAccess();
    void testWildcardAccess();
    void testRecursiveDescentAccess();
    void testFilterExpression();
    void testInvalidPath();
    void testComplexExample();
    void testIntegrationWithJSONPointer();
};

void JSONPathTest::testRootAccess()
{
    QJsonObject obj{{"foo", "bar"}};
    QJsonDocument doc(obj);

    JSONPath path("$");
    QVERIFY(path.isValid());

    QJsonArray result = path.evaluate(doc);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].toObject(), obj);
}

void JSONPathTest::testSimplePropertyAccess()
{
    QJsonObject obj{
        {"foo", "bar"},
        {"baz", 42}};
    QJsonDocument doc(obj);

    // Test accessing the "foo" property with dot notation
    JSONPath path1("$.foo");
    QVERIFY(path1.isValid());

    QJsonArray result1 = path1.evaluate(doc);
    QCOMPARE(result1.size(), 1);
    QCOMPARE(result1[0].toString(), QString("bar"));

    // Test accessing the "baz" property with bracket notation
    JSONPath path2("$['baz']");
    QVERIFY(path2.isValid());

    QJsonArray result2 = path2.evaluate(doc);
    QCOMPARE(result2.size(), 1);
    QCOMPARE(result2[0].toInt(), 42);
}

void JSONPathTest::testNestedPropertyAccess()
{
    QJsonObject innerObj{{"inner", "value"}};
    QJsonObject obj{
        {"nested", innerObj},
        {"foo", "bar"}};
    QJsonDocument doc(obj);

    // Test accessing the nested "inner" property
    JSONPath path("$.nested.inner");
    QVERIFY(path.isValid());

    QJsonArray result = path.evaluate(doc);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].toString(), QString("value"));
}

void JSONPathTest::testArrayIndexAccess()
{
    QJsonArray array = QJsonArray::fromVariantList({"zero", "one", "two", "three", "four"});
    QJsonDocument doc(array);

    // Test accessing specific indices
    JSONPath path1("$[1]");
    QVERIFY(path1.isValid());

    QJsonArray result1 = path1.evaluate(doc);
    QCOMPARE(result1.size(), 1);
    QCOMPARE(result1[0].toString(), QString("one"));

    // Test accessing with negative index
    JSONPath path2("$[-1]");
    QVERIFY(path2.isValid());

    QJsonArray result2 = path2.evaluate(doc);
    QCOMPARE(result2.size(), 1);
    QCOMPARE(result2[0].toString(), QString("four"));

    // Test accessing nested array inside object
    QJsonObject obj{{"arr", array}};
    QJsonDocument objDoc(obj);

    JSONPath path3("$.arr[2]");
    QVERIFY(path3.isValid());

    QJsonArray result3 = path3.evaluate(objDoc);
    QCOMPARE(result3.size(), 1);
    QCOMPARE(result3[0].toString(), QString("two"));
}

void JSONPathTest::testArraySliceAccess()
{
    QJsonArray array = QJsonArray::fromVariantList({"zero", "one", "two", "three", "four", "five"});
    QJsonDocument doc(array);

    // Test basic slice
    JSONPath path1("$[1:3]");
    QVERIFY(path1.isValid());

    QJsonArray result1 = path1.evaluate(doc);
    QCOMPARE(result1.size(), 2);
    QCOMPARE(result1[0].toString(), QString("one"));
    QCOMPARE(result1[1].toString(), QString("two"));

    // Test slice with step
    JSONPath path2("$[0:6:2]");
    QVERIFY(path2.isValid());

    QJsonArray result2 = path2.evaluate(doc);
    QCOMPARE(result2.size(), 3);
    QCOMPARE(result2[0].toString(), QString("zero"));
    QCOMPARE(result2[1].toString(), QString("two"));
    QCOMPARE(result2[2].toString(), QString("four"));

    // Test slice with negative indices
    JSONPath path3("$[-3:-1]");
    QVERIFY(path3.isValid());

    QJsonArray result3 = path3.evaluate(doc);
    QCOMPARE(result3.size(), 2);
    QCOMPARE(result3[0].toString(), QString("three"));
    QCOMPARE(result3[1].toString(), QString("four"));

    // Test slice with defaults
    JSONPath path4("$[2:]");
    QVERIFY(path4.isValid());

    QJsonArray result4 = path4.evaluate(doc);
    QCOMPARE(result4.size(), 4);
    QCOMPARE(result4[0].toString(), QString("two"));
    QCOMPARE(result4[3].toString(), QString("five"));
}

void JSONPathTest::testWildcardAccess()
{
    QJsonObject obj{
        {"prop1", "value1"},
        {"prop2", "value2"},
        {"prop3", "value3"}};
    QJsonDocument docObj(obj);

    // Test wildcard property access
    JSONPath path1("$.*");
    QVERIFY(path1.isValid());

    QJsonArray result1 = path1.evaluate(docObj);
    QCOMPARE(result1.size(), 3);

    // Test array wildcard access
    QJsonArray array = QJsonArray::fromVariantList({"zero", "one", "two"});
    QJsonDocument docArr(array);

    JSONPath path2("$[*]");
    QVERIFY(path2.isValid());

    QJsonArray result2 = path2.evaluate(docArr);
    QCOMPARE(result2.size(), 3);
    QCOMPARE(result2[0].toString(), QString("zero"));
    QCOMPARE(result2[1].toString(), QString("one"));
    QCOMPARE(result2[2].toString(), QString("two"));

    // Test nested wildcard access
    QJsonArray books = QJsonArray();
    books.append(QJsonObject{{"title", "Book 1"}, {"author", "Author 1"}});
    books.append(QJsonObject{{"title", "Book 2"}, {"author", "Author 2"}});
    books.append(QJsonObject{{"title", "Book 3"}, {"author", "Author 3"}});

    QJsonObject store{{"books", books}};
    QJsonDocument docStore(store);

    JSONPath path3("$.books[*].author");
    QVERIFY(path3.isValid());

    QJsonArray result3 = path3.evaluate(docStore);
    QCOMPARE(result3.size(), 3);
    QCOMPARE(result3[0].toString(), QString("Author 1"));
    QCOMPARE(result3[1].toString(), QString("Author 2"));
    QCOMPARE(result3[2].toString(), QString("Author 3"));
}

void JSONPathTest::testRecursiveDescentAccess()
{
    QJsonObject level3{{"id", "level3"}, {"value", "deepest"}};
    QJsonObject level2{{"id", "level2"}, {"value", "deeper"}, {"child", level3}};
    QJsonObject level1{{"id", "level1"}, {"value", "deep"}, {"child", level2}};
    QJsonObject root{{"id", "root"}, {"value", "top"}, {"child", level1}};

    QJsonDocument doc(root);

    // Test recursive descent to find all values
    JSONPath path1("$..value");
    QVERIFY(path1.isValid());

    QJsonArray result1 = path1.evaluate(doc);
    QCOMPARE(result1.size(), 4);
    // Values can be in any order due to recursive search
    QStringList values;
    for (int i = 0; i < result1.size(); ++i)
    {
        values.append(result1[i].toString());
    }
    QVERIFY(values.contains("top"));
    QVERIFY(values.contains("deep"));
    QVERIFY(values.contains("deeper"));
    QVERIFY(values.contains("deepest"));

    // Test recursive descent to find all id values
    JSONPath path2("$..id");
    QVERIFY(path2.isValid());

    QJsonArray result2 = path2.evaluate(doc);
    QCOMPARE(result2.size(), 4);
    QStringList ids;
    for (int i = 0; i < result2.size(); ++i)
    {
        ids.append(result2[i].toString());
    }
    QVERIFY(ids.contains("root"));
    QVERIFY(ids.contains("level1"));
    QVERIFY(ids.contains("level2"));
    QVERIFY(ids.contains("level3"));
}

void JSONPathTest::testFilterExpression()
{
    QJsonArray users = QJsonArray();
    users.append(QJsonObject{
        {"name", "John"},
        {"age", 30},
        {"active", true}});
    users.append(QJsonObject{
        {"name", "Jane"},
        {"age", 25},
        {"active", false}});
    users.append(QJsonObject{
        {"name", "Bob"},
        {"age", 40},
        {"active", true}});

    QJsonObject obj{{"users", users}};
    QJsonDocument doc(obj);

    // Test string equality filter
    JSONPath path1("$.users[?(@.name == 'Jane')].age");
    QVERIFY(path1.isValid());

    QJsonArray result1 = path1.evaluate(doc);
    QCOMPARE(result1.size(), 1);
    QCOMPARE(result1[0].toInt(), 25);

    // Test numeric comparison filter
    JSONPath path2("$.users[?(@.age > 30)].name");
    QVERIFY(path2.isValid());

    QJsonArray result2 = path2.evaluate(doc);
    QCOMPARE(result2.size(), 1);
    QCOMPARE(result2[0].toString(), QString("Bob"));
}

void JSONPathTest::testInvalidPath()
{
    // Test invalid path without root symbol
    JSONPath path1("foo.bar");
    QVERIFY(!path1.isValid());

    // Test syntactically invalid path
    JSONPath path2("$.[*]invalid");
    QVERIFY(!path2.isValid());
}

void JSONPathTest::testComplexExample()
{
    // Create a complex nested structure for testing
    QJsonArray categories = QJsonArray::fromStringList({"fiction", "classic"});

    QJsonArray books = QJsonArray();
    books.append(QJsonObject{
        {"id", "book1"},
        {"title", "Pride and Prejudice"},
        {"author", QJsonObject{
                       {"name", "Jane Austen"},
                       {"year", 1775}}},
        {"price", 9.99},
        {"categories", categories},
        {"inStock", true}});

    books.append(QJsonObject{
        {"id", "book2"},
        {"title", "To Kill a Mockingbird"},
        {"author", QJsonObject{
                       {"name", "Harper Lee"},
                       {"year", 1926}}},
        {"price", 12.99},
        {"categories", QJsonArray::fromStringList({"fiction", "classic", "pulitzer"})},
        {"inStock", false}});

    QJsonObject store{
        {"name", "Bookstore"},
        {"location", "New York"},
        {"inventory", books}};

    QJsonDocument doc(store);

    // Complex path combining multiple features
    JSONPath path("$.inventory[*][?(@.price < 10)].title");
    QVERIFY(path.isValid());

    QJsonArray result = path.evaluate(doc);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].toString(), QString("Pride and Prejudice"));
}

void JSONPathTest::testIntegrationWithJSONPointer()
{
    // Create a test document
    QJsonObject obj{
        {"name", "Test Store"},
        {"items", QJsonArray{
                      QJsonObject{{"id", 1}, {"name", "Item 1"}, {"price", 10.99}},
                      QJsonObject{{"id", 2}, {"name", "Item 2"}, {"price", 20.99}},
                      QJsonObject{{"id", 3}, {"name", "Item 3"}, {"price", 30.99}}}}};
    QJsonDocument doc(obj);

    // Compare JSONPointer and direct JSONPath for the same path
    JSONPointer pointer("/items/1/name");
    QJsonValue pointerResult = pointer.evaluate(doc);

    JSONPath path("$.items[1].name");
    QJsonArray pathResult = path.evaluate(doc);

    QCOMPARE(pathResult.size(), 1);
    QCOMPARE(pathResult[0], pointerResult);

    // Test JSONPath with complex operations
    JSONPath complexPath("$.items[?(@.price > 15)].name");
    QJsonArray complexResult = complexPath.evaluate(doc);

    QCOMPARE(complexResult.size(), 2);
    QCOMPARE(complexResult[0].toString(), QString("Item 2"));
    QCOMPARE(complexResult[1].toString(), QString("Item 3"));
}

QTEST_MAIN(JSONPathTest)