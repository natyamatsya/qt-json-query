// JaywayParityGTest.cpp - scaffold for porting Jayway JSONPath test cases
// This file will gradually fill with tests ported from the Java reference
// implementation.  For now it contains a placeholder that ensures the
// testsuite builds and is recognised by CTest.

#include <gtest/gtest.h>
#include "json-query/JSONPath.hpp"
#include <QJsonDocument>
#include <QJsonValue>

namespace {

TEST(JaywayParitySmoke, BuildWorks)
{
    // Trivial assertion so the test executable reports success.
    EXPECT_EQ(1, 1);
}

} // unnamed namespace
