#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include "json-query/json-path/JSONPath.hpp"
#include "framework/JSONMatchersGTest.hpp"

using json_query::JSONPath;

// The Java EscapeTest ensures that JSON provider does not escape forward slashes
// when serialising. Qt's QJsonDocument already leaves them untouched, so the
// semantics align.  This parity test verifies both the evaluated result and
// the round-trip serialisation using our matcher helpers.

namespace jayway_parity
{
using namespace ::testing;

TEST(JaywayEscapeParity, UrlsAreNotEscaped)
{
    constexpr const char* json = R"([
        "https://a/b/1",
        "https://a/b/2",
        "https://a/b/3"
    ])";

    // Identity path should yield the full array
    QJsonArray arr = evalArray(*JSONPath::create(u"$"), parseJson(json));
    EXPECT_THAT(
        arr, ElementsAre(IsJsonString("https://a/b/1"), IsJsonString("https://a/b/2"), IsJsonString("https://a/b/3")));

    // Serialise array and confirm slashes stay unescaped
    QByteArray serialised = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    EXPECT_TRUE(serialised.contains("https://a/b/1"));
}

} // namespace jayway_parity
