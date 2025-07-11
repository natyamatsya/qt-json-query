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

#include "JSONPointer.hpp"

class JSONPath
{
public:
    enum class Option { None = 0, AsPathList = 1 };
    explicit JSONPath(const QString &path, Option opt = Option::None);

    // Evaluate JSONPath against document or value. Returns:
    //  * Undefined QJsonValue if no match
    //  * Single matched scalar/object/array as-is if exactly one match
    //  * QJsonArray (wrapped into QJsonValue) when multiple matches
    QJsonValue evaluate(const QJsonDocument &document) const;
    QJsonValue evaluate(const QJsonValue &value) const;

    // Convenience: always returns an array (empty, single wrapped, or multiple)
    QJsonArray evaluateAll(const QJsonDocument &document) const;
    QJsonArray evaluateAll(const QJsonValue &value) const;

    bool isValid() const;
    QString toString() const;

public:
    enum class FunctionType { None, Length, Min, Max };
private:
    enum class SegmentType
    {
        Pointer,
        RecursiveDescend,
        WildProperty,
        ArrayWildcard,
        ArraySlice,
        FilterExpression
    };

    struct Segment
    {
        SegmentType type;
        std::variant<
            QString,
            std::tuple<int, int, int>,
            std::function<bool(const QJsonValue &)>> data;
    };

    bool m_valid = true;
    FunctionType m_func = FunctionType::None;
    Option m_option = Option::None;
    QString m_originalPath;
    QVector<Segment> m_segments;

    void parsePath(const QString &path);
    void detectTrailingFunction(QString &path);
    QVector<Segment> parseSegments(const QString &path);
    std::optional<Segment> parseFilterExpression(const QString &expr);

    QJsonArray evaluateSegment(const Segment &segment, const QJsonValue &value) const;
    QJsonArray evaluateRecursive(const QJsonValue &value, int segmentIndex) const;
    QJsonArray evaluateArraySlice(const QJsonArray &array, int start, int end, int step) const;
    int normalizeArrayIndex(int index, int arraySize) const;
    QJsonArray processWildcardProperty(const QJsonObject &obj) const;
    QJsonArray processWildcardArray(const QJsonArray &arr) const;
    QJsonArray recursiveDescend(const QJsonValue &value, const QString &property) const;
    QString segmentToPointer(const QString &segment) const;
    QVector<QPair<QString, bool>> splitPathSegments(const QString &path) const;
};
