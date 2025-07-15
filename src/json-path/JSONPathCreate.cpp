#include "JSONPath.hpp"

JSONPath::Result JSONPath::create(QStringView rawPath, Option opt)
{
    QString path = rawPath.toString();
    // Extract any trailing function → updates `path` and yields `func`
    FunctionType func = detectTrailingFunction(path);

    if (path.isEmpty())
        return std::unexpected(Error::EmptySegment); // choose your enum

    // Compile into tokens + filter list
    auto compiled = compilePath(path); // the expected<> helper
    if (!compiled)                     // → error bubbled up
        return std::unexpected(compiled.error());

    // Success: build the object
    return JSONPath(opt,
                    func,
                    rawPath.toString(),          // keep original as given
                    std::move(compiled->tokens), // tokens created in impl
                    std::move(compiled->filters));
}

JSONPath::FunctionType JSONPath::detectTrailingFunction(QString &path)
{
    using enum FunctionType;

    static const QPair<QString, FunctionType> table[] = {
        {".length()", Length},
        {".min()", Min},
        {".max()", Max},
    };

    for (auto &p : table)
        if (path.endsWith(p.first))
        {
            path.chop(p.first.size());
            return p.second;
        }

    return None;
}