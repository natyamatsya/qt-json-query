#pragma once

// Initial scaffolding for the upcoming JSONPath parser split.
// In the next step we will move the full path-parsing logic from
// JSONPath.cpp into PathParser.cpp and expose it via the `parse` free
// function below.

#include <QVector>
#include <QString>
#include <QJsonValue>
#include <variant>
#include <tuple>
#include <functional>

namespace json_path
{
    // Forward declarations – the concrete enum / struct definitions still
    // reside inside the existing JSONPath class. They will be migrated in
    // the next incremental refactor.
    enum class FunctionType { None, Length, Min, Max };

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

    struct ParseResult
    {
        QVector<Segment> segments;
        FunctionType     trailingFn = FunctionType::None;
        bool             valid      = true;
    };

    // Entry-point (currently stubbed).
    ParseResult parse(const QString &originalPath);
} // namespace json_path
