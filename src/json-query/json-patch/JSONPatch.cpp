// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-patch/JSONPatch.hpp"
#include "json-query/json-patch/JSONPatchError.hpp"
#include "json-query/json-patch/JSONPatchEquality.hpp"
#include "json-query/json-patch/JSONPatchParsing.hpp"
#include "json-query/json-pointer/JSONPointer.hpp"
#include "json-query/utils/JSONError.hpp"
#include "json-query/utils/detail/DocumentRoot.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>
#include <utility>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch::detail
{

namespace
{

[[nodiscard]] std::uint16_t clampOpIndex(qsizetype i) noexcept
{
    return static_cast<std::uint16_t>(std::min<qsizetype>(i, std::numeric_limits<std::uint16_t>::max()));
}

} // namespace

// RFC 6902 §4.6 value equality (declared in JSONPatchEquality.hpp; the
// compliance driver compares against the suite's expected documents with the
// same semantics the "test" operation uses).
bool jsonDeepEquals(const QJsonValue& a, const QJsonValue& b) noexcept
{
    if (a.isDouble() && b.isDouble())
        return a.toDouble() == b.toDouble();
    if (a.type() != b.type())
        return false;
    if (a.isObject())
    {
        // Copy-init, not brace-init (ADR-001)
        const QJsonObject objA = a.toObject();
        const QJsonObject objB = b.toObject();
        if (objA.size() != objB.size())
            return false;
        for (auto it = objA.constBegin(); it != objA.constEnd(); ++it)
        {
            const auto itB{objB.constFind(it.key())};
            if (itB == objB.constEnd() || !jsonDeepEquals(it.value(), itB.value()))
                return false;
        }
        return true;
    }
    if (a.isArray())
    {
        const QJsonArray arrA = a.toArray();
        const QJsonArray arrB = b.toArray();
        if (arrA.size() != arrB.size())
            return false;
        for (qsizetype i = 0; i < arrA.size(); ++i)
            if (!jsonDeepEquals(arrA.at(i), arrB.at(i)))
                return false;
        return true;
    }
    return a == b; // null, bool, string
}

namespace
{

// "from is a proper prefix of path" on the canonical string forms.
// to_string() re-encodes canonically (escaping, decimal indices), so plain
// string comparison with the '/' separator is exact.
[[nodiscard]] bool isProperPrefixOf(const json_pointer::JSONPointer& from, const json_pointer::JSONPointer& path)
{
    const QString fromStr{from.to_string()};
    const QString pathStr{path.to_string()};
    return pathStr.size() > fromStr.size() && pathStr.startsWith(fromStr) && pathStr.at(fromStr.size()) == u'/';
}

} // namespace

std::expected<void, DetailedParseError> parsePatch(const QJsonArray& patch, std::vector<Op>& ops) noexcept
{
    using enum ParseError;
    using json_pointer::JSONPointer;

    ops.clear();
    ops.reserve(patch.size());

    for (qsizetype i = 0; i < patch.size(); ++i)
    {
        const auto errIndex{clampOpIndex(i)};
        const auto entry{patch.at(i)};
        if (!entry.isObject())
            return std::unexpected(DetailedParseError{InvalidPatchDocument, errIndex});
        const QJsonObject opObj = entry.toObject(); // ADR-001: copy-init

        const auto opValue{opObj.value(QLatin1String("op"))};
        if (!opValue.isString())
            return std::unexpected(DetailedParseError{MissingOp, errIndex});

        Op::Kind   kind{};
        const auto opName{opValue.toString()};
        if (opName == QLatin1String("add"))
            kind = Op::Kind::Add;
        else if (opName == QLatin1String("copy"))
            kind = Op::Kind::Copy;
        else if (opName == QLatin1String("move"))
            kind = Op::Kind::Move;
        else if (opName == QLatin1String("remove"))
            kind = Op::Kind::Remove;
        else if (opName == QLatin1String("replace"))
            kind = Op::Kind::Replace;
        else if (opName == QLatin1String("test"))
            kind = Op::Kind::Test;
        else
            return std::unexpected(DetailedParseError{InvalidOperationName, errIndex});

        const auto pathValue{opObj.value(QLatin1String("path"))};
        if (!pathValue.isString())
            return std::unexpected(DetailedParseError{MissingPath, errIndex});
        auto path{JSONPointer::create(pathValue.toString())};
        if (!path)
            return std::unexpected(DetailedParseError{InvalidTargetPointer, errIndex});

        std::optional<JSONPointer> from;
        if (kind == Op::Kind::Move || kind == Op::Kind::Copy)
        {
            const auto fromValue{opObj.value(QLatin1String("from"))};
            if (!fromValue.isString())
                return std::unexpected(DetailedParseError{MissingFrom, errIndex});
            auto fromPtr{JSONPointer::create(fromValue.toString())};
            if (!fromPtr)
                return std::unexpected(DetailedParseError{InvalidFromPointer, errIndex});
            from = std::move(*fromPtr);
        }

        QJsonValue value;
        if (kind == Op::Kind::Add || kind == Op::Kind::Replace || kind == Op::Kind::Test)
        {
            // contains() rather than value().isUndefined(): an explicit JSON
            // null is a perfectly valid "value"
            if (!opObj.contains(QLatin1String("value")))
                return std::unexpected(DetailedParseError{MissingValue, errIndex});
            value = opObj.value(QLatin1String("value"));
        }

        // Aggregate init: Op has no default ctor (JSONPointer's is private)
        ops.push_back(Op{kind, std::move(*path), std::move(from), std::move(value)});
    }

    return {};
}

} // namespace json_query::json_patch::detail

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch
{

namespace
{

using detail::Op;

// Rewrite the detail of any error produced while applying an operation to the
// operation index — the pointer token index is meaningless to apply() callers.
[[nodiscard]] Error atOp(Error e, qsizetype opIndex) noexcept
{
    e.detail = static_cast<std::uint16_t>(
        std::min<qsizetype>(opIndex, std::numeric_limits<std::uint16_t>::max()));
    return e;
}

[[nodiscard]] std::expected<QJsonValue, Error> applyOps(const std::vector<Op>& ops, QJsonValue root) noexcept
{
    for (qsizetype i = 0; i < static_cast<qsizetype>(ops.size()); ++i)
    {
        const Op& op{ops[static_cast<std::size_t>(i)]};
        switch (op.kind)
        {
        case Op::Kind::Add:
            if (auto r{op.path.add(root, op.value)}; !r)
                return std::unexpected(atOp(r.error(), i));
            break;
        case Op::Kind::Replace:
            if (auto r{op.path.replace(root, op.value)}; !r)
                return std::unexpected(atOp(r.error(), i));
            break;
        case Op::Kind::Remove:
            if (auto r{op.path.remove(root)}; !r)
                return std::unexpected(atOp(r.error(), i));
            break;
        case Op::Kind::Copy:
        {
            auto value{op.from->evaluate(root)};
            if (!value)
                return std::unexpected(atOp(value.error(), i));
            if (auto r{op.path.add(root, *value)}; !r)
                return std::unexpected(atOp(r.error(), i));
            break;
        }
        case Op::Kind::Move:
        {
            // RFC 6902 §4.4: from must not be a proper prefix of path, and
            // the operation is remove(from) followed by add(path, removed)
            if (detail::isProperPrefixOf(*op.from, op.path))
                return std::unexpected(Error{EvalError::MoveIntoOwnDescendant, static_cast<std::uint16_t>(i)});
            auto removed{op.from->remove(root)};
            if (!removed)
                return std::unexpected(atOp(removed.error(), i));
            if (auto r{op.path.add(root, *removed)}; !r)
                return std::unexpected(atOp(r.error(), i));
            break;
        }
        case Op::Kind::Test:
        {
            auto value{op.path.evaluate(root)};
            if (!value)
                return std::unexpected(atOp(value.error(), i));
            if (!detail::jsonDeepEquals(*value, op.value))
                return std::unexpected(Error{EvalError::TestFailed, static_cast<std::uint16_t>(i)});
            break;
        }
        }
    }
    return root;
}

} // namespace

QJsonArray JSONPatch::toJson() const
{
    const auto opName = [](Op::Kind kind)
    {
        switch (kind)
        {
        case Op::Kind::Add:
            return QLatin1String("add");
        case Op::Kind::Copy:
            return QLatin1String("copy");
        case Op::Kind::Move:
            return QLatin1String("move");
        case Op::Kind::Remove:
            return QLatin1String("remove");
        case Op::Kind::Replace:
            return QLatin1String("replace");
        case Op::Kind::Test:
            return QLatin1String("test");
        }
        return QLatin1String("");
    };

    QJsonArray out;
    for (const Op& op : m_ops)
    {
        QJsonObject entry;
        entry.insert(QLatin1String("op"), opName(op.kind));
        entry.insert(QLatin1String("path"), op.path.to_string());
        if (op.from)
            entry.insert(QLatin1String("from"), op.from->to_string());
        if (op.kind == Op::Kind::Add || op.kind == Op::Kind::Replace || op.kind == Op::Kind::Test)
            entry.insert(QLatin1String("value"), op.value);
        out.append(entry);
    }
    return out;
}

JSONPatch::ParseResult JSONPatch::create(const QJsonArray& patch) noexcept
{
    JSONPatch jp;
    if (auto parseResult{detail::parsePatch(patch, jp.m_ops)}; !parseResult)
        return std::unexpected(Error{parseResult.error().error, parseResult.error().opIndex});
    return jp;
}

JSONPatch::ParseResult JSONPatch::create(const QJsonDocument& patch) noexcept
{
    if (!patch.isArray())
        return std::unexpected(Error{ParseError::InvalidPatchDocument});
    return create(patch.array());
}

JSONPatch::ApplyValueResult JSONPatch::apply(const QJsonValue& root) const noexcept
{
    return applyOps(m_ops, root);
}

JSONPatch::ApplyResult JSONPatch::apply(const QJsonDocument& document) const noexcept
{
    auto result{applyOps(m_ops, utils::detail::unwrapRoot(document))};
    if (!result)
        return std::unexpected(result.error());

    QJsonDocument out;
    if (!utils::detail::commitRoot(out, *result))
        // QJsonDocument cannot hold a scalar root (use the QJsonValue overload)
        return std::unexpected(Error{json_pointer::EvalError::DocumentRootNotContainer});
    return out;
}

std::expected<void, json_query::Error> JSONPatch::applyInPlace(QJsonDocument& document) const noexcept
{
    auto result{apply(document)};
    if (!result)
        return std::unexpected(result.error());
    document = std::move(*result);
    return {};
}

std::expected<void, json_query::Error> JSONPatch::applyInPlace(QJsonObject& root) const noexcept
{
    auto result{apply(QJsonValue{root})};
    if (!result)
        return std::unexpected(result.error());
    if (!result->isObject())
        return std::unexpected(Error{json_pointer::EvalError::RootTypeMismatch});
    root = result->toObject();
    return {};
}

std::expected<void, json_query::Error> JSONPatch::applyInPlace(QJsonArray& root) const noexcept
{
    auto result{apply(QJsonValue{root})};
    if (!result)
        return std::unexpected(result.error());
    if (!result->isArray())
        return std::unexpected(Error{json_pointer::EvalError::RootTypeMismatch});
    root = result->toArray();
    return {};
}

} // namespace json_query::json_patch
