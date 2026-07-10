// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-pointer/JSONPointer.hpp"
#include "json-query/json-pointer/JSONPointerWrite.hpp"
#include "json-query/utils/JSONError.hpp"
#include "json-query/utils/detail/DocumentRoot.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>
#include <type_traits>
#include <utility>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer::detail
{

namespace
{

// One level of the validated descent: the container met at this level and
// the position inside it that the walk descended through (or will write to).
struct PathStep
{
    QJsonValue   container;     // object or array (COW copy — refcount bump only)
    const Token* token;         // token applied inside `container`
    qsizetype    resolvedIndex; // >= 0: array position (== size means append); -1: object member
};

// RFC 6901 has no special "-" token; RFC 6902 §4.1 defines it as the
// (nonexistent) element after the last array element. It parses as a Key
// token and is interpreted container-relatively here.
[[nodiscard]] bool isAppendToken(const Token& tk) noexcept
{
    return tk.kind == Token::Kind::Key && tk.key.size() == 1 && tk.key.front() == u'-';
}

// Container type for a created intermediate, chosen by the token that will be
// applied inside it: array when that token can only address the first/append
// position of a fresh array, object otherwise.
[[nodiscard]] QJsonValue makeCreatedContainer(const Token& tk) noexcept
{
    if ((tk.kind == Token::Kind::Index && tk.index == 0) || isAppendToken(tk))
    {
        // Copy-init/default-init only — QJsonArray{...} would select the
        // initializer_list constructor and wrap the array (see ADR-001)
        QJsonArray arr;
        return QJsonValue{arr};
    }
    QJsonObject obj;
    return QJsonValue{obj};
}

[[nodiscard]] std::uint16_t clampTokenIndex(qsizetype i) noexcept
{
    return static_cast<std::uint16_t>(std::min<qsizetype>(i, std::numeric_limits<std::uint16_t>::max()));
}

// Rebuild the modified spine bottom-up: assign the changed child into a
// detached copy of each parent recorded during the descent. Untouched
// siblings remain COW-shared with the original document.
[[nodiscard]] QJsonValue unwind(const std::vector<PathStep>& path, QJsonValue leafContainer)
{
    QJsonValue child{std::move(leafContainer)};
    for (auto it = path.rbegin(); it != path.rend(); ++it)
    {
        if (it->resolvedIndex < 0)
        {
            QJsonObject obj = it->container.toObject();
            obj.insert(it->token->key, child);
            child = QJsonValue{obj};
        }
        else
        {
            // Copy-init, not brace-init (ADR-001)
            QJsonArray arr = it->container.toArray();
            if (it->resolvedIndex == arr.size())
                arr.append(child);
            else
                arr.replace(it->resolvedIndex, child);
            child = QJsonValue{arr};
        }
    }
    return child;
}

} // namespace

std::expected<QJsonValue, DetailedEvalError> writePointer(const std::vector<Token>& tokens,
                                                          QJsonValue&               root,
                                                          const QJsonValue&         value,
                                                          WriteOp                   op,
                                                          bool createIntermediates) noexcept
{
    using enum EvalError;

    // Inserting Undefined into a QJsonObject would *remove* the member;
    // normalize to JSON null (matching Qt's own array behavior).
    const QJsonValue newValue{value.isUndefined() ? QJsonValue{} : value};

    if (tokens.empty())
    {
        if (op == WriteOp::Remove)
            return std::unexpected(DetailedEvalError{CannotRemoveRoot, 0});
        root = newValue; // add/replace of "": the root always exists
        return QJsonValue{QJsonValue::Undefined};
    }

    std::vector<PathStep> path;
    path.reserve(tokens.size() - 1);
    QJsonValue current{root};

    for (qsizetype i{0};; ++i)
    {
        const Token& tk{tokens[static_cast<std::size_t>(i)]};
        const bool   leaf{i + 1 == static_cast<qsizetype>(tokens.size())};
        const auto   errIndex{clampTokenIndex(i)};

        // A JSON null counts as "nothing here yet": with createIntermediates
        // it becomes a container able to hold this token. Any other scalar is
        // never overwritten (falls through to the TypeMismatch below).
        if (current.isNull() && createIntermediates)
            current = makeCreatedContainer(tk);

        if (current.isObject())
        {
            QJsonObject obj = current.toObject(); // ADR-001: copy-init
            const auto  it{obj.constFind(tk.key)};
            const bool  exists{it != obj.constEnd()};

            if (leaf)
            {
                QJsonValue removed{QJsonValue::Undefined};
                switch (op)
                {
                case WriteOp::Add:
                    obj.insert(tk.key, newValue);
                    break;
                case WriteOp::Replace:
                    if (!exists)
                        return std::unexpected(DetailedEvalError{KeyNotFound, errIndex});
                    obj.insert(tk.key, newValue);
                    break;
                case WriteOp::Remove:
                    if (!exists)
                        return std::unexpected(DetailedEvalError{KeyNotFound, errIndex});
                    removed = *it;
                    obj.remove(tk.key);
                    break;
                }
                root = unwind(path, QJsonValue{obj});
                return removed;
            }

            if (exists)
            {
                path.push_back(PathStep{current, &tk, -1});
                current = *it;
            }
            else if (createIntermediates)
            {
                path.push_back(PathStep{current, &tk, -1});
                current = makeCreatedContainer(tokens[static_cast<std::size_t>(i) + 1]);
            }
            else
                return std::unexpected(DetailedEvalError{KeyNotFound, errIndex});
        }
        else if (current.isArray())
        {
            // Copy-init, not brace-init (ADR-001)
            QJsonArray arr = current.toArray();

            qsizetype idx{-1};
            if (tk.kind == Token::Kind::Index)
                idx = tk.index;
            else if (isAppendToken(tk))
                idx = arr.size(); // "-": the position after the last element
            else
                return std::unexpected(DetailedEvalError{TypeMismatchArray, errIndex});

            if (leaf)
            {
                QJsonValue removed{QJsonValue::Undefined};
                switch (op)
                {
                case WriteOp::Add:
                    if (idx > arr.size())
                        return std::unexpected(DetailedEvalError{IndexOutOfRange, errIndex});
                    arr.insert(idx, newValue); // idx == size appends
                    break;
                case WriteOp::Replace:
                    if (idx >= arr.size())
                        return std::unexpected(DetailedEvalError{IndexOutOfRange, errIndex});
                    arr.replace(idx, newValue);
                    break;
                case WriteOp::Remove:
                    if (idx >= arr.size())
                        return std::unexpected(DetailedEvalError{IndexOutOfRange, errIndex});
                    removed = arr.at(idx);
                    arr.removeAt(idx);
                    break;
                }
                root = unwind(path, QJsonValue{arr});
                return removed;
            }

            if (idx >= 0 && idx < arr.size())
            {
                path.push_back(PathStep{current, &tk, idx});
                current = arr.at(idx);
            }
            else if (createIntermediates && idx == arr.size())
            {
                // Append a created container (the only position addressable
                // beyond the existing elements; no null-padding)
                path.push_back(PathStep{current, &tk, idx});
                current = makeCreatedContainer(tokens[static_cast<std::size_t>(i) + 1]);
            }
            else
                return std::unexpected(DetailedEvalError{IndexOutOfRange, errIndex});
        }
        else
        {
            return std::unexpected(DetailedEvalError{TypeMismatchObject, errIndex});
        }
    }
}

} // namespace json_query::json_pointer::detail

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer
{

namespace
{

using detail::DetailedEvalError;
using detail::WriteOp;

[[nodiscard]] JSONPointer::WriteResult
toWriteResult(const std::expected<QJsonValue, DetailedEvalError>& result) noexcept
{
    if (!result)
        return std::unexpected(Error{result.error().error, result.error().tokenIndex});
    return {};
}

[[nodiscard]] JSONPointer::RemoveResult
toRemoveResult(std::expected<QJsonValue, DetailedEvalError>&& result) noexcept
{
    if (!result)
        return std::unexpected(Error{result.error().error, result.error().tokenIndex});
    return std::move(*result);
}

// Typed-root writes (QJsonObject& / QJsonArray&): the root's container kind
// is fixed, so a root-replacing write whose result is a different kind is
// RootTypeMismatch — and, like every other error, leaves the root untouched.
template <class Container>
[[nodiscard]] std::expected<QJsonValue, DetailedEvalError> writeTypedRoot(const std::vector<Token>& tokens,
                                                                          Container&                container,
                                                                          const QJsonValue&         value,
                                                                          WriteOp                   op,
                                                                          bool createIntermediates) noexcept
{
    QJsonValue root{container};

    auto result{detail::writePointer(tokens, root, value, op, createIntermediates)};
    if (!result)
        return result;

    if constexpr (std::is_same_v<Container, QJsonObject>)
    {
        if (!root.isObject())
            return std::unexpected(DetailedEvalError{EvalError::RootTypeMismatch, 0});
        container = root.toObject();
    }
    else
    {
        static_assert(std::is_same_v<Container, QJsonArray>);
        if (!root.isArray())
            return std::unexpected(DetailedEvalError{EvalError::RootTypeMismatch, 0});
        container = root.toArray();
    }
    return result;
}

// Unwrap the document root, run the write, and commit the result back.
// QJsonDocument can only hold an object/array (or be null): a write that
// leaves a scalar at the root is reported as DocumentRootNotContainer and —
// like every other error — leaves the document untouched.
[[nodiscard]] std::expected<QJsonValue, DetailedEvalError> writeDocument(const std::vector<Token>& tokens,
                                                                         QJsonDocument&            doc,
                                                                         const QJsonValue&         value,
                                                                         WriteOp                   op,
                                                                         bool createIntermediates) noexcept
{
    QJsonValue root{utils::detail::unwrapRoot(doc)};

    auto result{detail::writePointer(tokens, root, value, op, createIntermediates)};
    if (!result)
        return result;

    if (!utils::detail::commitRoot(doc, root))
        return std::unexpected(DetailedEvalError{EvalError::DocumentRootNotContainer, 0});

    return result;
}

} // namespace

JSONPointer::WriteResult JSONPointer::add(QJsonDocument& doc, const QJsonValue& value) const noexcept
{
    return toWriteResult(writeDocument(m_tokens, doc, value, WriteOp::Add, false));
}

JSONPointer::WriteResult JSONPointer::add(QJsonValue& root, const QJsonValue& value) const noexcept
{
    return toWriteResult(detail::writePointer(m_tokens, root, value, WriteOp::Add, false));
}

JSONPointer::WriteResult JSONPointer::replace(QJsonDocument& doc, const QJsonValue& value) const noexcept
{
    return toWriteResult(writeDocument(m_tokens, doc, value, WriteOp::Replace, false));
}

JSONPointer::WriteResult JSONPointer::replace(QJsonValue& root, const QJsonValue& value) const noexcept
{
    return toWriteResult(detail::writePointer(m_tokens, root, value, WriteOp::Replace, false));
}

JSONPointer::RemoveResult JSONPointer::remove(QJsonDocument& doc) const noexcept
{
    return toRemoveResult(writeDocument(m_tokens, doc, QJsonValue{}, WriteOp::Remove, false));
}

JSONPointer::RemoveResult JSONPointer::remove(QJsonValue& root) const noexcept
{
    return toRemoveResult(detail::writePointer(m_tokens, root, QJsonValue{}, WriteOp::Remove, false));
}

JSONPointer::WriteResult JSONPointer::set(QJsonDocument& doc, const QJsonValue& value, WriteOptions options) const noexcept
{
    return toWriteResult(writeDocument(m_tokens, doc, value, WriteOp::Add, options.createIntermediates));
}

JSONPointer::WriteResult JSONPointer::set(QJsonValue& root, const QJsonValue& value, WriteOptions options) const noexcept
{
    return toWriteResult(detail::writePointer(m_tokens, root, value, WriteOp::Add, options.createIntermediates));
}

// ─── Typed-root overloads (QJsonObject& / QJsonArray&) ─────────────────────

JSONPointer::WriteResult JSONPointer::add(QJsonObject& root, const QJsonValue& value) const noexcept
{
    return toWriteResult(writeTypedRoot(m_tokens, root, value, WriteOp::Add, false));
}

JSONPointer::WriteResult JSONPointer::add(QJsonArray& root, const QJsonValue& value) const noexcept
{
    return toWriteResult(writeTypedRoot(m_tokens, root, value, WriteOp::Add, false));
}

JSONPointer::WriteResult JSONPointer::replace(QJsonObject& root, const QJsonValue& value) const noexcept
{
    return toWriteResult(writeTypedRoot(m_tokens, root, value, WriteOp::Replace, false));
}

JSONPointer::WriteResult JSONPointer::replace(QJsonArray& root, const QJsonValue& value) const noexcept
{
    return toWriteResult(writeTypedRoot(m_tokens, root, value, WriteOp::Replace, false));
}

JSONPointer::RemoveResult JSONPointer::remove(QJsonObject& root) const noexcept
{
    return toRemoveResult(writeTypedRoot(m_tokens, root, QJsonValue{}, WriteOp::Remove, false));
}

JSONPointer::RemoveResult JSONPointer::remove(QJsonArray& root) const noexcept
{
    return toRemoveResult(writeTypedRoot(m_tokens, root, QJsonValue{}, WriteOp::Remove, false));
}

JSONPointer::WriteResult JSONPointer::set(QJsonObject& root, const QJsonValue& value, WriteOptions options) const noexcept
{
    return toWriteResult(writeTypedRoot(m_tokens, root, value, WriteOp::Add, options.createIntermediates));
}

JSONPointer::WriteResult JSONPointer::set(QJsonArray& root, const QJsonValue& value, WriteOptions options) const noexcept
{
    return toWriteResult(writeTypedRoot(m_tokens, root, value, WriteOp::Add, options.createIntermediates));
}

} // namespace json_query::json_pointer
