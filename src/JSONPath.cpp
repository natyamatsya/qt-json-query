// jsonpath.cpp - Using CTRE
#include "json-query/JSONPath.h"
#include <QStack>
#include "json-query/JSONQueryUtils.hpp"


// ----------------
// CTRE Patterns
// ----------------
namespace
{
    using namespace json_utils;

    // Define all regex patterns at compile-time
    constexpr auto special_operator_pattern = ctll::fixed_string{
        "(\\.\\.|\\.\\*|\\[\\*\\]|\\[\\d*:\\-?\\d*(?::\\d+)?\\]|\\[\\?\\(.*?\\)\\])"};

    // Extract array slice components
    constexpr auto slice_components_pattern = ctll::fixed_string{
        "\\[(\\d*):(\\-?\\d*)(?::(\\d+))?\\]"};

    // Extract filter expression
    constexpr auto filter_expr_pattern = ctll::fixed_string{
        "\\[\\?\\((.+?)\\)\\]"};
}

JSONPath::JSONPath(const QString &path)
{
    parsePath(path);
}

void JSONPath::parsePath(const QString &path)
{
    if (path.isEmpty())
    {
        m_valid = false;
        return;
    }

    // JSONPath must start with $ or @
    if (!path.startsWith('$') && !path.startsWith('@'))
    {
        m_valid = false;
        return;
    }

    try
    {
        m_segments = parseSegments(path);
    }
    catch (...)
    {
        m_valid = false;
    }
}

QVector<JSONPath::Segment> JSONPath::parseSegments(const QString &path)
{
    QVector<Segment> segments;

    // Split the path into segments using compile-time regex
    auto splitSegments = splitPathSegments(path);

    // First segment must be the root
    if (splitSegments.isEmpty() || !json_utils::matches<json_utils::root_pattern>(splitSegments.first().first))
    {
        throw std::runtime_error("Invalid JSONPath: Must start with $ or @");
    }

    // Add the root segment
    Segment rootSegment;
    rootSegment.type = SegmentType::Pointer;
    rootSegment.data = QString("$");
    segments.append(rootSegment);

    // Process remaining segments
    for (int i = 1; i < splitSegments.size(); ++i)
    {
        const auto &[segment, isSpecial] = splitSegments[i];

        if (!isSpecial)
        {
            // Direct path segment - convert to JSONPointer format
            Segment directSegment;
            directSegment.type = SegmentType::Pointer;
            directSegment.data = segmentToPointer(segment);
            segments.append(directSegment);
        }
        else
        {
            // Special operator segment
            using namespace json_utils;

            if (ctre::match<recursive_descent_pattern>(to_sv(segment)))
            {
                // Recursive descent
                Segment recursiveSegment;
                recursiveSegment.type = SegmentType::RecursiveDescend;
                segments.append(recursiveSegment);
            }
            else if (auto propertyMatch = ctre::match<property_pattern>(to_sv(segment));
                     propertyMatch && to_qstr(propertyMatch.template get<1>().to_view()) == "*")
            {
                // Wildcard property
                Segment wildcardSegment;
                wildcardSegment.type = SegmentType::WildProperty;
                segments.append(wildcardSegment);
            }
            else if (ctre::match<array_wildcard_pattern>(to_sv(segment)))
            {
                // Array wildcard
                Segment wildcardSegment;
                wildcardSegment.type = SegmentType::ArrayWildcard;
                segments.append(wildcardSegment);
            }
            else if (auto sliceMatch = ctre::match<slice_components_pattern>(to_sv(segment)))
            {
                // Array slice
                int start = 0;
                int end = std::numeric_limits<int>::max();
                int step = 1;

                if (sliceMatch.template get<1>())
                {
                    start = QString::fromUtf8(sliceMatch.template get<1>().data(), sliceMatch.template get<1>().size()).toInt();
                }

                if (sliceMatch.template get<2>())
                {
                    auto endStr = QString::fromUtf8(sliceMatch.template get<2>().data(), sliceMatch.template get<2>().size());
                    if (!endStr.isEmpty())
                    {
                        end = endStr.toInt();
                    }
                }

                if (sliceMatch.template get<3>())
                {
                    auto stepStr = QString::fromUtf8(sliceMatch.template get<3>().data(), sliceMatch.template get<3>().size());
                    if (!stepStr.isEmpty())
                    {
                        step = stepStr.toInt();
                        if (step <= 0)
                            step = 1; // Ensure positive step
                    }
                }

                Segment sliceSegment;
                sliceSegment.type = SegmentType::ArraySlice;
                sliceSegment.data = std::make_tuple(start, end, step);
                segments.append(sliceSegment);
            }
            else if (auto filterMatch = ctre::match<filter_expr_pattern>(to_sv(segment)))
            {
                // Filter expression
                if (filterMatch.template get<1>())
                {
                    QString expr = to_qstr(filterMatch.template get<1>().to_view());
                    auto filterSegment = parseFilterExpression(expr);
                    if (filterSegment.has_value())
                    {
                        segments.append(filterSegment.value());
                    }
                }
            }
        }
    }

    return segments;
}

QVector<QPair<QString, bool>> JSONPath::splitPathSegments(const QString &path) const
{
    using namespace json_utils;

    QVector<QPair<QString, bool>> segments;
    std::string_view sv = to_sv(path);

    // Find all special operators
    auto matches = find_all_positions<special_operator_pattern>(path);

    int lastPos = 0;
    for (const auto &[start, end] : matches)
    {
        // Add any direct path segment before the special operator
        if (start > lastPos)
        {
            segments.append({path.mid(lastPos, start - lastPos), false});
        }

        // Add the special operator
        segments.append({path.mid(start, end - start), true});

        lastPos = end;
    }

    // Add any remaining direct path segment
    if (lastPos < path.length())
    {
        segments.append({path.mid(lastPos), false});
    }

    return segments;
}

std::optional<JSONPath::Segment> JSONPath::parseFilterExpression(const QString &expr)
{
    using namespace json_utils;

    Segment segment;
    segment.type = SegmentType::FilterExpression;

    // Parse a simple equality expression using CTRE
    if (auto eqMatch = ctre::match<eq_expr_pattern>(to_sv(expr)))
    {
        QString property = to_qstr(eqMatch.template get<1>().to_view());
        QString value = to_qstr(eqMatch.template get<2>().to_view());

        segment.data = [property, value](const QJsonValue &json) -> bool
        {
            if (!json.isObject())
                return false;
            QJsonObject obj = json.toObject();
            return obj.contains(property) && obj[property].toString() == value;
        };

        return segment;
    }

    // Parse a simple numeric comparison using CTRE
    if (auto numMatch = ctre::match<num_comp_expr_pattern>(to_sv(expr)))
    {
        QString property = to_qstr(numMatch.template get<1>().to_view());
        QString op = to_qstr(numMatch.template get<2>().to_view());
        double compareValue = to_qstr(numMatch.template get<3>().to_view()).toDouble();

        segment.data = [property, op, compareValue](const QJsonValue &json) -> bool
        {
            if (!json.isObject())
                return false;
            QJsonObject obj = json.toObject();
            if (!obj.contains(property) || !obj[property].isDouble())
                return false;

            double value = obj[property].toDouble();

            if (op == ">")
                return value > compareValue;
            if (op == "<")
                return value < compareValue;
            if (op == ">=")
                return value >= compareValue;
            if (op == "<=")
                return value <= compareValue;

            return false;
        };

        return segment;
    }

    return std::nullopt;
}

QJsonArray JSONPath::evaluate(const QJsonDocument &document) const
{
    return evaluate(document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array()));
}

QJsonArray JSONPath::evaluate(const QJsonValue &value) const
{
    if (!isValid() || m_segments.isEmpty())
    {
        return QJsonArray();
    }

    QJsonArray result;
    result.append(value); // Start with the root

    // Evaluate each segment
    for (int i = 1; i < m_segments.size(); ++i)
    {
        QJsonArray newResult;

        for (int j = 0; j < result.size(); ++j)
        {
            QJsonArray segmentResult = evaluateSegment(m_segments[i], result[j]);
            for (const QJsonValue &val : segmentResult)
            {
                newResult.append(val);
            }
        }

        result = newResult;
    }

    return result;
}

QJsonArray JSONPath::evaluateSegment(const Segment &segment, const QJsonValue &value) const
{
    QJsonArray result;

    switch (segment.type)
    {
    case SegmentType::Pointer:
    {
        QString pointerPath = std::get<QString>(segment.data);

        // Evaluate the pointer
        JSONPointer pointer(pointerPath);
        QJsonValue pointerResult = pointer.evaluate(value);

        if (!pointerResult.isUndefined())
        {
            result.append(pointerResult);
        }
        break;
    }

    case SegmentType::RecursiveDescend:
    {
        // Recursive descent requires knowledge of the next segment
        // In this simplified version, we'll just look for any matching properties recursively
        if (value.isObject() || value.isArray())
        {
            result = evaluateRecursive(value, 0);
        }
        break;
    }

    case SegmentType::WildProperty:
    {
        if (value.isObject())
        {
            result = processWildcardProperty(value.toObject());
        }
        break;
    }

    case SegmentType::ArrayWildcard:
    {
        if (value.isArray())
        {
            result = processWildcardArray(value.toArray());
        }
        break;
    }

    case SegmentType::ArraySlice:
    {
        if (value.isArray())
        {
            auto [start, end, step] = std::get<std::tuple<int, int, int>>(segment.data);
            result = evaluateArraySlice(value.toArray(), start, end, step);
        }
        break;
    }

    case SegmentType::FilterExpression:
    {
        if (value.isArray())
        {
            QJsonArray arr = value.toArray();
            auto predicate = std::get<std::function<bool(const QJsonValue &)>>(segment.data);

            for (int i = 0; i < arr.size(); ++i)
            {
                if (predicate(arr[i]))
                {
                    result.append(arr[i]);
                }
            }
        }
        break;
    }
    }

    return result;
}

QJsonArray JSONPath::evaluateRecursive(const QJsonValue &value, int segmentIndex) const
{
    QJsonArray result;

    // Recursive implementation for the .. operator
    if (value.isObject())
    {
        QJsonObject obj = value.toObject();

        // Add all values at this level
        result = processWildcardProperty(obj);

        // Recursively process all child values
        for (auto it = obj.begin(); it != obj.end(); ++it)
        {
            QJsonArray childMatches = evaluateRecursive(it.value(), segmentIndex);
            for (int i = 0; i < childMatches.size(); ++i)
            {
                result.append(childMatches[i]);
            }
        }
    }
    else if (value.isArray())
    {
        QJsonArray arr = value.toArray();

        // Recursively process all elements
        for (int i = 0; i < arr.size(); ++i)
        {
            QJsonArray childMatches = evaluateRecursive(arr[i], segmentIndex);
            for (int j = 0; j < childMatches.size(); ++j)
            {
                result.append(childMatches[j]);
            }
        }
    }

    return result;
}

QJsonArray JSONPath::evaluateArraySlice(const QJsonArray &array, int start, int end, int step) const
{
    QJsonArray result;

    if (step <= 0)
    {
        return result; // Invalid step
    }

    int size = array.size();

    // Normalize start and end indices
    start = normalizeArrayIndex(start, size);

    // Special case for end = MAX_INT
    if (end == std::numeric_limits<int>::max())
    {
        end = size;
    }
    else
    {
        end = normalizeArrayIndex(end, size);
    }

    // Apply slice
    for (int i = start; i < end && i < size; i += step)
    {
        if (i >= 0)
        {
            result.append(array[i]);
        }
    }

    return result;
}

int JSONPath::normalizeArrayIndex(int index, int arraySize) const
{
    // Handle negative indices (counting from the end)
    if (index < 0)
    {
        index = arraySize + index;
    }
    return index;
}

QJsonArray JSONPath::processWildcardProperty(const QJsonObject &obj) const
{
    QJsonArray result;

    // Add all values from the object
    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        result.append(it.value());
    }

    return result;
}

QJsonArray JSONPath::processWildcardArray(const QJsonArray &arr) const
{
    QJsonArray result;

    // Add all elements from the array
    for (int i = 0; i < arr.size(); ++i)
    {
        result.append(arr[i]);
    }

    return result;
}

bool JSONPath::isValid() const
{
    return m_valid;
}

QString JSONPath::toString() const
{
    if (!isValid())
    {
        return QString();
    }

    // Building the string representation is complicated and depends on the segment types
    // This is a simplified implementation
    return QString("$"); // Placeholder
}

QString JSONPath::segmentToPointer(const QString &segment) const
{
    using namespace json_utils;

    QString result = segment;

    // Remove $ root symbol
    if (result.startsWith('$'))
    {
        result.remove(0, 1);
    }

    // Convert dotted notation to JSONPointer format using CTRE
    auto propMatches = find_all_positions<property_pattern>(result);
    for (int i = propMatches.size() - 1; i >= 0; --i)
    {
        const auto &[start, end] = propMatches[i];
        auto propMatch = ctre::match<property_pattern>(to_sv(result.mid(start, end - start)));
        if (propMatch.template get<1>())
        {
            QString propName = to_qstr(propMatch.template get<1>().to_view());
            result.replace(start, end - start, "/" + propName);
        }
    }

    // Convert array notation using CTRE
    auto arrayMatches = find_all_positions<array_index_pattern>(result);
    for (int i = arrayMatches.size() - 1; i >= 0; --i)
    {
        const auto &[start, end] = arrayMatches[i];
        auto arrayMatch = ctre::match<array_index_pattern>(to_sv(result.mid(start, end - start)));
        if (arrayMatch.template get<1>())
        {
            QString indexStr = to_qstr(arrayMatch.template get<1>().to_view());
            result.replace(start, end - start, "/" + indexStr);
        }
    }

    // Handle quoted property notation using CTRE
    auto quotedMatches = find_all_positions<quoted_property_pattern>(result);
    for (int i = quotedMatches.size() - 1; i >= 0; --i)
    {
        const auto &[start, end] = quotedMatches[i];
        auto quotedMatch = ctre::match<quoted_property_pattern>(to_sv(result.mid(start, end - start)));
        if (quotedMatch.template get<1>())
        {
            QString propName = to_qstr(quotedMatch.template get<1>().to_view());
            result.replace(start, end - start, "/" + propName);
        }
    }

    return result;
}
