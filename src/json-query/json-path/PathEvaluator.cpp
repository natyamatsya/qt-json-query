#include "json-query/json-path/PathEvaluator.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp" // for pure helpers

namespace json_query::json_path::detail {

// --------------------------------------------------------------
// Basic helpers (free versions copied from legacy JSONPath.cpp)
// --------------------------------------------------------------
int normalizeIndex(int idx, int size)
{
    return idx < 0 ? size + idx : idx;
}

QJsonArray evalSlice(const QJsonArray& array, const Slice& s)
{
    QJsonArray out;
    if (s.step <= 0) return out;

    const int size = array.size();
    auto norm = [size](int i) { return i < 0 ? size + i : i; };

    int begin = norm(static_cast<int>(s.start));
    int end   = (s.end == std::numeric_limits<qsizetype>::max())
                ? size
                : norm(static_cast<int>(s.end));

    for (int i = begin; i < end && i < size; i += static_cast<int>(s.step))
        if (i >= 0) out.append(array[i]);
    return out;
}

// --------------------------------------------------------------
// Core evaluation entry points (bridge implementation)
// --------------------------------------------------------------
QJsonValue evaluate(const PathEvalCtx& ctx,
                    const json_query::JSONPath& self,
                    const QJsonValue& root)
{
    // Bridge: still delegate to legacy implementations using JSONPath.
    if (ctx.option == json_query::JSONPath::Option::AsPathList)
        return self.evalAsPathList(self, root);      // friend method
    return self.evalStandard(self, root);
}

QJsonArray evaluateAll(const PathEvalCtx& ctx,
                       const json_query::JSONPath& self,
                       const QJsonValue& root)
{
    // Re-use existing helper which already returns flattened array
    return json_path::detail::evaluateAll(self, root);
}

} // namespace json_query::json_path::detail
