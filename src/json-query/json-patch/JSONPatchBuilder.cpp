// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-patch/JSONPatchBuilder.hpp"

#include <QJsonObject>
#include <QLatin1String>
#include <QString>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch
{

namespace
{

// The builder assembles wire-format op objects; compiled pointers re-encode
// canonically via to_string(), raw strings pass through and are validated by
// JSONPatch::create() in build(). Two shapes cover all six operations:
// path (+ optional value), and from + path.

[[nodiscard]] QJsonObject valueOp(QLatin1String op, QStringView path, const QJsonValue& value)
{
    QJsonObject entry;
    entry.insert(QLatin1String("op"), op);
    entry.insert(QLatin1String("path"), path.toString());
    entry.insert(QLatin1String("value"), value);
    return entry;
}

[[nodiscard]] QJsonObject fromOp(QLatin1String op, QStringView from, QStringView path)
{
    QJsonObject entry;
    entry.insert(QLatin1String("op"), op);
    entry.insert(QLatin1String("path"), path.toString());
    entry.insert(QLatin1String("from"), from.toString());
    return entry;
}

} // namespace

JSONPatchBuilder& JSONPatchBuilder::add(const json_pointer::JSONPointer& path, const QJsonValue& value)
{
    return add(QStringView{path.to_string()}, value);
}

JSONPatchBuilder& JSONPatchBuilder::add(QStringView path, const QJsonValue& value)
{
    m_ops.append(valueOp(QLatin1String("add"), path, value));
    return *this;
}

JSONPatchBuilder& JSONPatchBuilder::replace(const json_pointer::JSONPointer& path, const QJsonValue& value)
{
    return replace(QStringView{path.to_string()}, value);
}

JSONPatchBuilder& JSONPatchBuilder::replace(QStringView path, const QJsonValue& value)
{
    m_ops.append(valueOp(QLatin1String("replace"), path, value));
    return *this;
}

JSONPatchBuilder& JSONPatchBuilder::remove(const json_pointer::JSONPointer& path)
{
    return remove(QStringView{path.to_string()});
}

JSONPatchBuilder& JSONPatchBuilder::remove(QStringView path)
{
    QJsonObject entry;
    entry.insert(QLatin1String("op"), QLatin1String("remove"));
    entry.insert(QLatin1String("path"), path.toString());
    m_ops.append(entry);
    return *this;
}

JSONPatchBuilder& JSONPatchBuilder::move(const json_pointer::JSONPointer& from, const json_pointer::JSONPointer& path)
{
    return move(QStringView{from.to_string()}, QStringView{path.to_string()});
}

JSONPatchBuilder& JSONPatchBuilder::move(QStringView from, QStringView path)
{
    m_ops.append(fromOp(QLatin1String("move"), from, path));
    return *this;
}

JSONPatchBuilder& JSONPatchBuilder::copy(const json_pointer::JSONPointer& from, const json_pointer::JSONPointer& path)
{
    return copy(QStringView{from.to_string()}, QStringView{path.to_string()});
}

JSONPatchBuilder& JSONPatchBuilder::copy(QStringView from, QStringView path)
{
    m_ops.append(fromOp(QLatin1String("copy"), from, path));
    return *this;
}

JSONPatchBuilder& JSONPatchBuilder::test(const json_pointer::JSONPointer& path, const QJsonValue& value)
{
    return test(QStringView{path.to_string()}, value);
}

JSONPatchBuilder& JSONPatchBuilder::test(QStringView path, const QJsonValue& value)
{
    m_ops.append(valueOp(QLatin1String("test"), path, value));
    return *this;
}

JSONPatch::ParseResult JSONPatchBuilder::build() const noexcept
{
    return JSONPatch::create(m_ops);
}

} // namespace json_query::json_patch
