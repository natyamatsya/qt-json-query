// jsonpath.cpp - Using CTRE
#include "json-query/JSONPath.hpp"
#include <vector>
#include "json-query/JSONQueryUtils.hpp"
#include "json-query/ContainerFrame.hpp"
using json_query::ContainerFrame;


// ----------------
// CTRE Patterns
// ----------------
namespace
{
    using namespace json_utils;

    // Define all regex patterns at compile-time
    constexpr auto special_operator_pattern = ctll::fixed_string{
        "(\\.\\.|\\.\\*|\\[\\*\\]|\\[\\-?\\d*:\\-?\\d*(?::\\d+)?\\]|\\[\\?\\(.*?\\)\\])"};

    // Extract array slice components
    constexpr auto slice_components_pattern = ctll::fixed_string{
        "\\[(\\-?\\d*):(\\-?\\d*)(?::(\\d+))?\\]"};

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

    // Ensure we have at least the root symbol
    if (splitSegments.isEmpty())
    {
        throw std::runtime_error("Invalid JSONPath: Empty path");
    }

    // Normalise so the root symbol ('$' or '@') is always its own segment
    QVector<QPair<QString, bool>> normalizedSegments;
    const QString &firstPart = splitSegments.first().first;

    if (!firstPart.startsWith('$') && !firstPart.startsWith('@'))
    {
        throw std::runtime_error("Invalid JSONPath: Must start with $ or @");
    }

    // Always push the root symbol as a standalone segment
    normalizedSegments.append({firstPart.left(1), false});

    // If the first part has additional characters beyond the root symbol (e.g. "$.books")
    if (firstPart.length() > 1)
    {
        QString remainder = firstPart.mid(1);
        if (!remainder.isEmpty())
        {
            normalizedSegments.append({remainder, false});
        }
    }

    // Append the rest of the original split segments
    for (int i = 1; i < splitSegments.size(); ++i)
    {
        normalizedSegments.append(splitSegments[i]);
    }

    // Replace with the normalised vector for further processing
    splitSegments = std::move(normalizedSegments);

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
            // Direct path segment validation
            if (segment.isEmpty() || segment == ".")
            {
                throw std::runtime_error("Invalid JSONPath: Invalid segment detected");
            }
            // Convert to JSONPointer format
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
                // If the segment starts with "..", split it so that the recursive descent is a standalone
                // segment and the remainder (if any) is processed normally. E.g. "..value" -> ["..", "value"].
                Segment recursiveSegment;
                recursiveSegment.type = SegmentType::RecursiveDescend;
                segments.append(recursiveSegment);

                // Remainder after the leading ".."
                QString remainder = segment.mid(2);
                if (remainder.startsWith('.'))
                {
                    remainder.remove(0, 1);
                }
                if (!remainder.isEmpty())
                {
                    Segment directSeg;
                    directSeg.type = SegmentType::Pointer;
                    directSeg.data = segmentToPointer(remainder);
                    segments.append(directSeg);
                }
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
            else
            {
                // Unsupported or malformed operator
                throw std::runtime_error("Invalid or unsupported JSONPath operator: " + segment.toStdString());
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
        QString op = to_qstr(numMatch.template get<2>().to_view()).trimmed();
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
        if (result.isEmpty())
            break; // Early exit if no candidates remain

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

        // Handle array indices (including negative) directly to support negative indexing
        if (value.isArray() && pointerPath.startsWith('/'))
        {
            bool ok = false;
            int idx = pointerPath.mid(1).toInt(&ok);
            if (ok)
            {
                QJsonArray arr = value.toArray();
                int normIdx = normalizeArrayIndex(idx, arr.size());
                if (normIdx >= 0 && normIdx < arr.size())
                {
                    result.append(arr[normIdx]);
                    break; // Done handling pointer
                }
            }
        }

        // Fallback to JSONPointer evaluation (for object properties and positive indices)
        JSONPointer pointer(pointerPath);
        QJsonValue pointerResult = pointer.evaluate(value);

        if (!pointerResult.isUndefined())
        {
            result.append(pointerResult);
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

    case SegmentType::RecursiveDescend:
    {
        // Return all descendant containers (objects/arrays) including current container.
        // This allows the subsequent segment to evaluate over each descendant.
        result = evaluateRecursive(value, 0);
        break;
    }

    case SegmentType::FilterExpression:
    {
        {
            auto predicate = std::get<std::function<bool(const QJsonValue &)>>(segment.data);
            if (value.isArray())
            {
                QJsonArray arr = value.toArray();
                for (const QJsonValue &elem : arr)
                {
                    if (predicate(elem))
                    {
                        result.append(elem);
                    }
                }
            }
            else
            {
                // Value is a single element (object). Apply predicate directly.
                if (predicate(value))
                {
                    result.append(value);
                }
            }
        }
        break;
    }
    }

    return result;
}



QJsonArray JSONPath::evaluateRecursive(const QJsonValue &value, int /*segmentIndex*/) const
{
    QJsonArray result;

    if (value.isNull() || value.isUndefined())
        return result;

    if (!value.isObject() && !value.isArray())
        return result;

    std::vector<ContainerFrame> stack;
    stack.reserve(32);

    // Push root frame & store root container itself
    if (value.isObject())
    {
        result.append(value);
        stack.emplace_back(value.toObject());
    }
    else // array
    {
        result.append(value);
        stack.emplace_back(value.toArray());
    }

    while (!stack.empty())
    {
        ContainerFrame &frame = stack.back();
        if (!frame.hasNext())
        {
            stack.pop_back();
            continue;
        }

        QJsonValue child = frame.next();
        if (child.isObject())
        {
            result.append(child);
            stack.emplace_back(child.toObject());
        }
        else if (child.isArray())
        {
            result.append(child);
            stack.emplace_back(child.toArray());
        }
    }
    return result;
}

QJsonArray JSONPath::evaluateArraySlice(const QJsonArray &array, int start, int end, int step) const
{
    QJsonArray result;
    if (step <= 0)
        return result;

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
    // Fast conversion of a direct JSONPath segment to RFC-6901 JSON Pointer.
    // Supports dot-notation properties and bracketed array indices/quoted properties.

    if (segment.isEmpty() || segment == "$")
        return QString();

    QStringView sv{segment};
    qsizetype pos = 0;

    // Skip the root symbol if present.
    if (sv.at(0) == u'$')
        ++pos;

    QString out;
    out.reserve(sv.size()); // worst-case length

    auto flushProperty = [&](qsizetype start, qsizetype end)
    {
        if (end > start)
        {
            out += u'/';
            out += sv.sliced(start, end - start);
        }
    };

    while (pos < sv.size())
    {
        const QChar c = sv.at(pos);

        if (c == u'.')
        {
            ++pos; // delimiter between properties
            continue;
        }

        if (c == u'[')
        {
            ++pos; // enter bracket
            if (pos >= sv.size()) break;

            // Quoted property ['prop'] or ["prop"]
            if (sv.at(pos) == u'\'' || sv.at(pos) == u'\"')
            {
                const QChar quote = sv.at(pos);
                ++pos;
                qsizetype start = pos;
                while (pos < sv.size() && sv.at(pos) != quote)
                    ++pos;
                flushProperty(start, pos);
                // skip closing quote and optional bracket
                while (pos < sv.size() && sv.at(pos) != u']')
                    ++pos;
                ++pos; // skip ']'
                continue;
            }

            // Numeric (or negative) array index
            qsizetype start = pos;
            while (pos < sv.size() && sv.at(pos) != u']')
                ++pos;
            flushProperty(start, pos);
            ++pos; // skip ']'
            continue;
        }

        // Plain property until next '.' or '['
        qsizetype start = pos;
        while (pos < sv.size() && sv.at(pos) != u'.' && sv.at(pos) != u'[')
            ++pos;
        flushProperty(start, pos);
    }

    return out;
}
