#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>

namespace json_query {
    class JSONPath; // Forward declaration
}

namespace json_query::json_path::detail {

/**
 * @brief Core evaluation functions for JSONPath
 * 
 * This module contains the fundamental evaluation strategies used by JSONPath
 * to process JSON structures, including wildcard expansion and recursive descent.
 */

/**
 * @brief Evaluate JSONPath against a document and return all matches as array
 * @param jp The JSONPath instance
 * @param document The JSON document to evaluate against
 * @return QJsonArray containing all matching values
 */
QJsonArray evaluateAll(const json_query::JSONPath& jp, const QJsonDocument& document);

/**
 * @brief Evaluate JSONPath against a value and return all matches as array
 * @param jp The JSONPath instance
 * @param value The JSON value to evaluate against
 * @return QJsonArray containing all matching values
 */
QJsonArray evaluateAll(const json_query::JSONPath& jp, const QJsonValue& value);

/**
 * @brief Expand wildcard (*) for JSON objects - returns all property values
 * @param jp The JSONPath instance (for context)
 * @param obj The JSON object to expand
 * @return QJsonArray containing all property values
 */
QJsonArray wildcardObject(const json_query::JSONPath& jp, const QJsonObject& obj);

/**
 * @brief Expand wildcard (*) for JSON arrays - returns the array itself
 * @param jp The JSONPath instance (for context)
 * @param arr The JSON array to expand
 * @return QJsonArray (shallow copy of input array)
 */
QJsonArray wildcardArray(const json_query::JSONPath& jp, const QJsonArray& arr);

/**
 * @brief Recursive descent evaluation (..) - collect all descendant containers
 * @param jp The JSONPath instance (for context)
 * @param value The JSON value to recursively traverse
 * @param unused Unused parameter (kept for compatibility)
 * @return QJsonArray containing all descendant containers
 */
QJsonArray evaluateRecursive(const json_query::JSONPath& jp, const QJsonValue& value, int unused = 0);

// New pure overloads (phase A) – no JSONPath dependency
QJsonArray wildcardObject(const QJsonObject& obj);
QJsonArray wildcardArray(const QJsonArray& arr);
QJsonArray evaluateRecursive(const QJsonValue& value, int unused = 0);

} // namespace json_query::json_path::detail
