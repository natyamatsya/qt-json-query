// JaywayParityGTestHelpers.hpp
// Common helper utilities and lightweight matchers shared by the C++ Jayway
// parity test suite.  The goal is to mimic the convenience found in the
// original Java tests (BaseTest, TestUtils, AssertJ chainable assertions) while
// staying minimal and dependency-free beyond GoogleTest & Qt.
//
// Usage:
//   #include "JaywayParityGTestHelpers.hpp"
//   auto doc  = jp::parseJson(R"({"foo": "bar"})");
//   auto path = *JSONPath::create(u"$.foo");
//   QJsonValue v = jp::eval(path, doc);
//   EXPECT_TRUE(jp::IsString(v, u"bar"));
//
//   QJsonArray arr = jp::evalArray(*JSONPath::create(u"$[*]"), doc);
//   EXPECT_TRUE(jp::ContainsOnly(arr, 1, 2, 3));
// ---------------------------------------------------------------------------

#ifndef JAYWAY_PARITY_GTEST_HELPERS_HPP
#define JAYWAY_PARITY_GTEST_HELPERS_HPP

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <utility>
#include <initializer_list>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <vector>
#include <expected>
#include "json-query/JSONPath.hpp"

namespace jp {
// Parse raw JSON C-string into a QJsonDocument.
inline QJsonDocument parseJson(const char* src)
{
    return QJsonDocument::fromJson(QByteArray(src));
}

// ---------------------------------------------------------------------------
// Evaluation wrappers --------------------------------------------------------
// ---------------------------------------------------------------------------

// Evaluate a compiled JSONPath against a document and return the raw value.
inline QJsonValue eval(const JSONPath& path, const QJsonDocument& doc)
{
    return path.evaluate(doc);
}

// Convenience overload that compiles the path each call (avoid in hot code).
inline QJsonValue eval(QStringView path, const QJsonDocument& doc)
{
    auto p = JSONPath::create(path);
    if (!p) return {};
    return p->evaluate(doc);
}

// Return result as array – scalar values are wrapped into single-element array
// so caller can treat uniformly.
inline QJsonArray evalArray(const JSONPath& path, const QJsonDocument& doc)
{
    QJsonValue v = path.evaluate(doc);
    return v.isArray() ? v.toArray() : QJsonArray{v};
}

// ---------------------------------------------------------------------------
// Exception expectation ------------------------------------------------------
// ---------------------------------------------------------------------------
// Transition from throw-based to std::expected tests.  Provide helpers that
// check for specific json_query::Error codes instead of exceptions.

using EvalResult = std::expected<QJsonValue, json_query::Error>;

// Evaluate path and propagate compile-time errors via std::expected.  Runtime
// evaluation currently cannot fail (always returns a value), so we wrap it in
// a successful result for now.  When evaluation gains error handling this
// wrapper will forward those as well.
inline EvalResult evalExp(QStringView path,
                          const QJsonDocument& doc,
                          JSONPath::Option opt = JSONPath::Option::None)
{
    auto compiled = JSONPath::create(path, opt);
    if (!compiled)
        return std::unexpected(compiled.error());
    return compiled->evaluate(doc);
}

// Macro to assert that a statement of type std::expected<> fails with a
// specific json_query::Error code.
#define EXPECT_PATH_ERROR(expr, errEnum)                        \
    do {                                                        \
        auto _res = (expr);                                    \
        ASSERT_FALSE(_res.has_value())                         \
            << "Expected error but got success";              \
        EXPECT_EQ(_res.error(), (errEnum));                    \
    } while (false)

} // namespace jp

// ---------------------------------------------------------------------------
// Custom GoogleTest matchers ------------------------------------------------

// Matcher for JSON string equality
MATCHER_P(IsJsonString, expected, "JSON string equals")
{
    return arg.isString() && arg.toString() == expected;
}

MATCHER_P(IsJsonInt, expected, "JSON int equals")
{
    return arg.isDouble() && arg.toInt() == expected;
}

MATCHER_P(JsonObjContains, kvPairs, "object contains key/value pairs")
{
    if (!arg.isObject()) return false;
    const auto obj = arg.toObject();
    for (const auto &pair : kvPairs)
    {
        const QString &key = pair.first;
        const QJsonValue &val = pair.second;
        if (!obj.contains(key) || obj.value(key) != val)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------

// Verify QJsonValue is string and equals expected.
inline ::testing::AssertionResult IsString(const QJsonValue& v, QStringView expected)
{
    if (!v.isString())
        return ::testing::AssertionFailure() << "value is not a JSON string";
    if (v.toString() != expected)
        return ::testing::AssertionFailure() << "expected '" << expected.toString().toStdString()
                                            << "' but got '" << v.toString().toStdString() << "'";
    return ::testing::AssertionSuccess();
}

// Helper to compare that an array contains exactly the provided elements in
// any order (no duplicates unless provided).
inline bool containsAll(const QJsonArray& arr, const std::vector<QJsonValue>& expected)
{
    for (const auto& e : expected)
        if (!arr.contains(e)) return false;
    return true;
}

template <typename... T>
::testing::AssertionResult ContainsOnly(const QJsonArray& arr, const T&... elems)
{
    std::vector<QJsonValue> expected{ QJsonValue(elems)... };
    if (arr.size() != int(expected.size()))
        return ::testing::AssertionFailure() << "array size " << arr.size()
                                            << " differs from expected " << expected.size();
    if (!containsAll(arr, expected))
        return ::testing::AssertionFailure() << "array does not contain exactly the expected elements";
    return ::testing::AssertionSuccess();
}


#endif // JAYWAY_PARITY_GTEST_HELPERS_HPP
