#pragma once

#include <QStringView>
#include <QVector>
#include <expected>
#include <functional>

class QJsonValue;

namespace json_query::json_path
{
    // Forward declarations from JSONPathCompile.hpp
    struct Token;
    enum class Error : std::uint8_t;

    namespace detail {
        // Forward declaration for KeyBuilder
        class KeyBuilder;

        // ──────────────────────────────────────────────────────────────────────
        //  Core Parser Functions
        // ──────────────────────────────────────────────────────────────────────

        /**
         * Parse dot-segment notation (., .., .identifier)
         * @param pos Current position in the string view
         * @param sv String view being parsed
         * @param kb Key builder for token creation
         * @param tokens Output vector for generated tokens
         * @return Next position or error
         */
        std::expected<qsizetype, Error> parseDot(qsizetype pos, QStringView sv,
                                                KeyBuilder& kb, QVector<Token>& tokens);

        /**
         * Parse bare identifier (unquoted name)
         * @param pos Current position in the string view
         * @param sv String view being parsed
         * @param kb Key builder for token creation
         * @return Next position or error
         */
        std::expected<qsizetype, Error> parseBare(qsizetype pos, QStringView sv, KeyBuilder& kb);

        /**
         * Parse bracket notation [...]
         * @param pos Current position in the string view
         * @param sv String view being parsed
         * @param kb Key builder for token creation
         * @param tokens Output vector for generated tokens
         * @param contextFilters Output vector for context-aware filters
         * @param filters Output vector for regular filters
         * @return Next position or error
         */
        std::expected<qsizetype, Error> parseBracket(qsizetype pos, QStringView sv,
                                                    KeyBuilder& kb, QVector<Token>& tokens,
                                                    QVector<std::function<bool (const QJsonValue&, const QJsonValue&)>>& contextFilters,
                                                    QVector<std::function<bool (const QJsonValue&)>>& filters);

    } // namespace detail

} // namespace json_query::json_path
