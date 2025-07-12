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
#include "json-path/PathParser.hpp"

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

private:
    static QJsonValue evalAsPathList(const JSONPath &self, const QJsonValue &value);
    static QJsonValue evalStandard(const JSONPath &self, const QJsonValue &value);

public:
    // Type aliases imported from json_path:: namespace (see PathParser.hpp)
    using FunctionType = json_path::FunctionType;
private:
    using SegmentType = json_path::SegmentType;
    using Segment     = json_path::Segment;

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
