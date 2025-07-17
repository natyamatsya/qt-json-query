// EscapeParityGTest.cpp
// Parity representation for Jayway EscapeTest.java

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonArray>
#include "json-query/JSONPath.hpp"
#include "JaywayParityGTestHelpers.hpp"

// The Java EscapeTest ensures that JSON provider does not escape forward slashes
// when serialising. Qt's QJsonDocument always escapes only control chars so
// behaviour already matches. Implement a quick check, but mark disabled until
// we formalise serialisation helper.

TEST(JaywayEscapeParity, UrlsAreNotEscaped)
{
    // JSON array of URLs as in Java EscapeTest
    const char* json = R"([
        "https://a/b/1",
        "https://a/b/2",
        "https://a/b/3"
    ])";

    // Evaluate path "$" (identity) which should return whole array
    auto doc = jp::parseJson(json);
    ASSERT_TRUE(!doc.isNull());

    auto path = JSONPath::create(u"$");
    ASSERT_TRUE(path);
    QJsonValue result = path->evaluate(doc);
    ASSERT_TRUE(result.isArray());

    // Serialise back to JSON and verify slashes remain unescaped
    QByteArray serialised = QJsonDocument(result.toArray()).toJson(QJsonDocument::Compact);
    EXPECT_TRUE(serialised.contains("https://a/b/1"));
}
