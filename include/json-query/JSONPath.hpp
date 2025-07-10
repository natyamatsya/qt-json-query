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
    explicit JSONPath(const QString &path);

    QJsonArray evaluate(const QJsonDocument &document) const;
    QJsonArray evaluate(const QJsonValue &value) const;

    bool isValid() const;
    QString toString() const;

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
    QVector<Segment> m_segments;

    void parsePath(const QString &path);
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
