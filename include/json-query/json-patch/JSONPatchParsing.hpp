// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "JSONPatchError.hpp"
#include "../json-pointer/JSONPointer.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch::detail
{

// One validated RFC 6902 operation. Pointers are compiled eagerly by
// JSONPatch::create(), so apply() never parses.
struct Op
{
    enum class Kind : quint8
    {
        Add,
        Copy,
        Move,
        Remove,
        Replace,
        Test,
    };

    Kind                                     kind{};
    json_pointer::JSONPointer                path;
    std::optional<json_pointer::JSONPointer> from{};  // move/copy only
    QJsonValue                               value{}; // add/replace/test only
};

// Internal error carrying the parse error and the index of the offending
// operation in the patch array (0 for whole-document errors).
struct DetailedParseError
{
    ParseError    error{};
    std::uint16_t opIndex{};
};

// Parse and validate a patch document into compiled operations. Defined in
// JSONPatch.cpp (no inline definition needed: only the JSONPatch factory
// calls this).
[[nodiscard]] std::expected<void, DetailedParseError> parsePatch(const QJsonArray& patch,
                                                                 std::vector<Op>&  ops) noexcept;

} // namespace json_query::json_patch::detail
