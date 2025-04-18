#pragma once

#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <optional>
#include <variant>
#include <ctre.hpp>

#include "JSONPointer.h"

class JSONPath
{
public:
    // Constructs a JSONPath from a string representation (e.g., "$.store.book[*].author")
    explicit JSONPath(const QString &path);

    // Evaluates the path against a JSON document
    QJsonArray evaluate(const QJsonDocument &document) const;

    // Evaluates the path against a JSON value
    QJsonArray evaluate(const QJsonValue &value) const;

    // Returns true if the path is valid
    bool isValid() const;

    // Returns the string representation of the path
    QString toString() const;

private:
    enum class SegmentType
    {
        Pointer,          // Direct path (evaluated using JSONPointer)
        RecursiveDescend, // ..
        WildProperty,     // .*
        ArrayWildcard,    // [*]
        ArraySlice,       // [start:end:step]
        FilterExpression  // [?(...)]
    };

    struct Segment
    {
        SegmentType type;
        std::variant<
            QString,                                // For Pointer
            std::tuple<int, int, int>,              // For ArraySlice (start, end, step)
            std::function<bool(const QJsonValue &)> // For FilterExpression
            >
            data;
    };

    bool m_valid = true;
    QVector<Segment> m_segments;

    // Helper methods for parsing
    void parsePath(const QString &path);
    QVector<Segment> parseSegments(const QString &path);
    std::optional<Segment> parseFilterExpression(const QString &expr);

    // Helper methods for evaluation
    QJsonArray evaluateSegment(const Segment &segment, const QJsonValue &value) const;
    QJsonArray evaluateRecursive(const QJsonValue &value, int segmentIndex) const;
    QJsonArray evaluateArraySlice(const QJsonArray &array, int start, int end, int step) const;

    // Helper method to normalize array index (handling negative indices)
    int normalizeArrayIndex(int index, int arraySize) const;

    // Helper method to handle wildcards
    QJsonArray processWildcardProperty(const QJsonObject &obj) const;
    QJsonArray processWildcardArray(const QJsonArray &arr) const;

    // Helper method for recursive descent
    QJsonArray recursiveDescend(const QJsonValue &value, const QString &property) const;

    // Convert a JSONPath segment to a JSONPointer where possible
    QString segmentToPointer(const QString &segment) const;

    // Function to split path into special operators and direct path segments using CTRE
    QVector<QPair<QString, bool>> splitPathSegments(const QString &path) const;
};

#endif // JSONPATH_H
