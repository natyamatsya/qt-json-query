// jsonpath.cpp - Using CTRE
#include "json-query/JSONPath.hpp"
#include <vector>
#include <regex>
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
    QString pathMod = path;
    detectTrailingFunction(pathMod);
    if (pathMod.isEmpty())
    {
        m_valid = false;
        return;
    }

    // JSONPath must start with $ or @
    if (!pathMod.startsWith('$') && !pathMod.startsWith('@'))
    {
        m_valid = false;
        return;
    }

    try
    {
        m_segments = parseSegments(pathMod);
    }
    catch (...)
    {
        m_valid = false;
    }
}

void JSONPath::detectTrailingFunction(QString &path)
{
    static const QPair<QString, FunctionType> funcs[] = {
        {".length()", FunctionType::Length},
        {".min()", FunctionType::Min},
        {".max()", FunctionType::Max},
    };
    for (const auto &p : funcs)
    {
        if (path.endsWith(p.first))
        {
            m_func = p.second;
            path.chop(p.first.size());
            return;
        }
    }
    m_func = FunctionType::None;
}


QVector<JSONPath::Segment> JSONPath::parseSegments(const QString &path)
{
    QVector<Segment> segments;
    segments.reserve(path.count('.') + path.count('[') + 4);

    QStringView sv{path};
    qsizetype pos = 0;
    const qsizetype n = sv.size();

    if (n == 0 || (sv.at(0) != u'$' && sv.at(0) != u'@'))
        throw std::runtime_error("Invalid JSONPath: Must start with $ or @");

    Segment root;
    root.type = SegmentType::Pointer;
    root.data = QString(sv.mid(0,1));
    segments.append(std::move(root));
    pos = 1;

    auto pushPointer = [&](QStringView segmentView)
    {
        if (segmentView.isEmpty())
            throw std::runtime_error("Invalid JSONPath: Empty segment");
        Segment s;
        s.type = SegmentType::Pointer;
        s.data = segmentToPointer(QString(segmentView));
        segments.append(std::move(s));
    };

    while (pos < n)
    {
        const QChar c = sv.at(pos);
        if (c == u'.')
        {
            if (pos + 1 >= n)
                throw std::runtime_error("Trailing '.' in JSONPath");
            QChar next = sv.at(pos + 1);
            if (next == u'.')
            {
                Segment rec{SegmentType::RecursiveDescend, {}};
                segments.append(std::move(rec));
                pos += 2;
                continue;
            }
            if (next == u'*')
            {
                Segment wild{SegmentType::WildProperty, {}};
                segments.append(std::move(wild));
                pos += 2;
                continue;
            }
            ++pos;
            qsizetype start = pos;
            while (pos < n && sv.at(pos) != u'.' && sv.at(pos) != u'[')
                ++pos;
            pushPointer(sv.sliced(start, pos - start));
            continue;
        }
        else if (c == u'[')
        {
            ++pos;
            qsizetype start = pos;
            int bracketDepth = 1;
            while (pos < n && bracketDepth > 0)
            {
                if (sv.at(pos) == u'[') ++bracketDepth;
                else if (sv.at(pos) == u']') --bracketDepth;
                ++pos;
            }
            if (bracketDepth != 0)
                throw std::runtime_error("Unmatched '[' in JSONPath");
            qsizetype end = pos - 1;
            QStringView content = sv.sliced(start, end - start);

            if (content == u"*")
            {
                Segment aw;
                aw.type = SegmentType::ArrayWildcard;
                segments.append(std::move(aw));
                continue;
            }

            if (!content.isEmpty() && (content.at(0) == u'?'))
            {
                QString expr = QString(content);
                auto optSeg = parseFilterExpression(expr);
                if (!optSeg)
                    throw std::runtime_error("Unsupported filter expression");
                segments.append(std::move(*optSeg));
                continue;
            }

            if (content.indexOf(u':') != -1)
            {
                auto takeInt = [](QStringView v, int defaultVal) -> int {
                    if (v.isEmpty()) return defaultVal;
                    bool ok = false;
                    int res = v.toInt(&ok);
                    return ok ? res : defaultVal;
                };
                qsizetype firstColon = content.indexOf(u':');
                qsizetype secondColon = content.indexOf(u':', firstColon + 1);
                QStringView startStr = content.sliced(0, firstColon);
                QStringView endStr;
                QStringView stepStr;
                if (secondColon == -1)
                {
                    endStr = content.sliced(firstColon + 1);
                }
                else
                {
                    endStr = content.sliced(firstColon + 1, secondColon - firstColon - 1);
                    stepStr = content.sliced(secondColon + 1);
                }
                int startInt = takeInt(startStr, 0);
                int endInt = takeInt(endStr, std::numeric_limits<int>::max());
                int stepInt = takeInt(stepStr, 1);
                Segment slice;
                slice.type = SegmentType::ArraySlice;
                slice.data = std::make_tuple(startInt, endInt, stepInt);
                segments.append(std::move(slice));
                continue;
            }

            if (!content.isEmpty() && (content.at(0) == u'\'' || content.at(0) == u'\"'))
            {
                QChar quote = content.at(0);
                qsizetype qEnd = content.lastIndexOf(quote);
                if (qEnd <= 0)
                    throw std::runtime_error("Unmatched quote in bracket property");
                QStringView prop = content.sliced(1, qEnd - 1);
                pushPointer(prop);
                continue;
            }

            QString bracketSeg = "[" + QString(content) + "]";
            pushPointer(bracketSeg);
            continue;
        }
        else
        {
            qsizetype start = pos;
            while (pos < n && sv.at(pos) != u'.' && sv.at(pos) != u'[')
                ++pos;
            pushPointer(sv.sliced(start, pos - start));
            continue;
        }
    }

    return segments;
}

QVector<QPair<QString, bool>> JSONPath::splitPathSegments(const QString &path) const
{
    // Manual scan that identifies special JSONPath operators without regex or extra allocations.
    // A segment is marked `true` (special) if it is one of:
    //   1. Recursive descent  ".."
    //   2. Wildcard property ".*"
    //   3. Any bracket expression  "[ ... ]"  (array index, slice, wildcard, filter, etc.)

    QVector<QPair<QString, bool>> segments;
    const qsizetype len = path.size();

    qsizetype lastPos = 0;
    qsizetype pos = 0;

    auto addPlain = [&](qsizetype from, qsizetype to)
    {
        if (to > from)
            segments.append({path.mid(from, to - from), false});
    };

    while (pos < len)
    {
        const QChar c = path.at(pos);

        if (c == u'.')
        {
            // Possible special operator: ".." or ".*"
            if (pos + 1 < len && (path.at(pos + 1) == u'.' || path.at(pos + 1) == u'*'))
            {
                addPlain(lastPos, pos);
                // special operator length 2
                segments.append({path.mid(pos, 2), true});
                pos += 2;
                lastPos = pos;
                continue;
            }
            // Single dot is just a delimiter between plain property names
            ++pos;
            continue;
        }
        else if (c == u'[')
        {
            // Bracket expression – treat entire bracketed part as special operator
            addPlain(lastPos, pos);
            qsizetype endBracket = path.indexOf(u']', pos + 1);
            if (endBracket == -1)
            {
                // Malformed input: take rest of string as special to avoid endless loop
                endBracket = len - 1;
            }
            segments.append({path.mid(pos, endBracket - pos + 1), true});
            pos = endBracket + 1;
            lastPos = pos;
            continue;
        }
        else
        {
            ++pos;
        }
    }

    // trailing plain segment
    addPlain(lastPos, len);

    return segments;
}

std::optional<JSONPath::Segment> JSONPath::parseFilterExpression(const QString &expr)
{
    using namespace json_utils;

    Segment segment;
    segment.type = SegmentType::FilterExpression;
    // Normalize expression: remove optional leading '?' and enclosing parentheses
    QString exprTrim = expr.trimmed();
    if (exprTrim.startsWith(u'?'))
        exprTrim.remove(0, 1);
    // If expression is wrapped in parentheses, strip them
    if (exprTrim.startsWith(u'(') && exprTrim.endsWith(u')'))
        exprTrim = exprTrim.mid(1, exprTrim.size() - 2);


    // Parse regex predicate @.prop =~ /pattern/
    {
        constexpr auto regex_pred_pattern = ctll::fixed_string{R"(@\.(\w+)\s*=~\s*/([^/]*)/)"};
        if (auto m = ctre::match<regex_pred_pattern>(to_sv(exprTrim)))
        {
            QString property = to_qstr(m.template get<1>().to_view());
            QString pattern  = to_qstr(m.template get<2>().to_view());
            std::regex re(pattern.toStdString());

            segment.data = [property, re](const QJsonValue &json) -> bool {
                if (!json.isObject()) return false;
                QJsonObject obj = json.toObject();
                if (!obj.contains(property) || !obj[property].isString()) return false;
                return std::regex_search(obj[property].toString().toStdString(), re);
            };
            return segment;
        }
    }

    // Parse dot-notation predicate like @.id == 2  or @.price > 20.0 via CTRE
        {
        constexpr auto dot_pred_pattern = ctll::fixed_string{R"(@\.(\w+)\s*(==|!=|>=|<=|>|<)\s*(.+))"};
        if (auto m = ctre::match<dot_pred_pattern>(to_sv(exprTrim)))
        {
            QString property = to_qstr(m.template get<1>().to_view());
            QString op       = to_qstr(m.template get<2>().to_view());
            QString rawVal   = to_qstr(m.template get<3>().to_view());
            rawVal = rawVal.trimmed();

            bool numeric = false;
            double compareValue = rawVal.toDouble(&numeric);
            QString compareStr;
            if (!numeric)
            {
                // Strip surrounding quotes if present
                if (rawVal.size() >= 2 && ((rawVal.startsWith('"') && rawVal.endsWith('"')) || (rawVal.startsWith('\'') && rawVal.endsWith('\''))))
                    compareStr = rawVal.mid(1, rawVal.size() - 2);
                else
                    compareStr = rawVal;
            }

            segment.data = [property, op, numeric, compareValue, compareStr](const QJsonValue &json) -> bool
            {
                if (!json.isObject())
                    return false;
                QJsonObject obj = json.toObject();
                if (!obj.contains(property))
                    return false;

                if (numeric)
                {
                    if (!obj[property].isDouble())
                        return false;
                    double value = obj[property].toDouble();
                    if (op == "==") return value == compareValue;
                    if (op == "!=") return value != compareValue;
                    if (op == ">")  return value > compareValue;
                    if (op == "<")  return value < compareValue;
                    if (op == ">=") return value >= compareValue;
                    if (op == "<=") return value <= compareValue;
                    return false;
                }
                else
                {
                    QString valStr = obj[property].toString();
                    if (op == "==") return valStr == compareStr;
                    if (op == "!=") return valStr != compareStr;
                    return false;
                }
            };
            return segment;
        }
    }

    // Parse a simple equality expression using CTRE
    if (auto eqMatch = ctre::match<eq_expr_pattern>(to_sv(exprTrim)))
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
    if (auto numMatch = ctre::match<json_utils::num_comp_expr_pattern>(to_sv(exprTrim)))
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

QJsonValue JSONPath::evaluate(const QJsonDocument &document) const
{
    const QJsonValue root = document.isArray() ? QJsonValue(document.array())
                                               : QJsonValue(document.object());
    return evaluate(root);
}

QJsonArray JSONPath::evaluateAll(const QJsonDocument &document) const
{
    return evaluateAll(document.isArray() ? QJsonValue(document.array())
                                          : QJsonValue(document.object()));
}

QJsonArray JSONPath::evaluateAll(const QJsonValue &value) const
{
    QJsonValue res = evaluate(value);
    if (res.isArray())
        return res.toArray();
    if (res.isUndefined() || res.isNull())
        return {};
    return QJsonArray{res};
}

QJsonValue JSONPath::evaluate(const QJsonValue &value) const
{
    if (!isValid() || m_segments.isEmpty())
        return QJsonValue();

    // Special case: path is just "$"
    if (m_segments.size() == 1)
        return value;

    QJsonArray working;
    working.append(value);

    bool hadMultiSelect = false;
    for (int i = 1; i < m_segments.size(); ++i)
    {
        const SegmentType t = m_segments[i].type;
        if (t == SegmentType::FilterExpression || t == SegmentType::ArrayWildcard ||
            t == SegmentType::WildProperty || t == SegmentType::ArraySlice)
            hadMultiSelect = true;
        if (working.isEmpty())
            break;
        QJsonArray next;
        for (const QJsonValue &candidate : working)
        {
            QJsonArray segVals = evaluateSegment(m_segments[i], candidate);
            for (const QJsonValue &v : segVals)
                next.append(v);
        }
        working = std::move(next);
    }

    if (working.isEmpty())
        return QJsonValue();
    if (m_func == FunctionType::None && working.size() == 1 && !hadMultiSelect)
        return working.first();
    QJsonValue out;
    if (m_func == FunctionType::None)
        out = (working.size()==1 && !hadMultiSelect) ? working.first() : QJsonValue(working);
    else
        out = QJsonValue(working);
    // Apply trailing function if any
    if (m_func != FunctionType::None)
    {
        if (m_func == FunctionType::Length)
        {
            // If the first (and only) match is itself an array, return its length
            if (working.size()==1 && working.first().isArray())
                return QJsonValue(static_cast<double>(working.first().toArray().size()));
            // Otherwise return the number of matched items
            return QJsonValue(static_cast<double>(working.size()));
        }
        if (m_func == FunctionType::Min || m_func == FunctionType::Max)
        {
            if (!out.isArray())
                return QJsonValue();
            const QJsonArray arr = out.toArray();
            if (arr.isEmpty())
                return QJsonValue();
            double best = arr.first().toDouble();
            for (const QJsonValue &v : arr)
            {
                if (!v.isDouble()) continue;
                double d = v.toDouble();
                if (m_func == FunctionType::Min)
                    best = std::min(best, d);
                else
                    best = std::max(best, d);
            }
            return QJsonValue(best);
        }
    }
    return out;
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
