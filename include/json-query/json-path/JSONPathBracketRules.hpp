#pragma once

#include <QStringView>
#include <QVector>
#include <QJsonValue>
#include <functional>
#include <expected>
#include <vector>

namespace json_query::json_path
{
    // Forward declarations from JSONPathCompile.hpp
    struct Token;
    struct Slice;
    enum class Error : std::uint8_t;

    namespace detail {
        // Forward declaration for KeyBuilder
        class KeyBuilder;

        // ──────────────────────────────────────────────────────────────────────
        //  BracketSink - Token Emission Facade
        // ──────────────────────────────────────────────────────────────────────

        /**
         * Facade for consistent token emission during bracket parsing
         * Handles bracket group metadata and filter management
         */
        class BracketSink {
        public:
            QVector<Token>&   tk;
            KeyBuilder&       kb;
            QVector<std::function<bool (const QJsonValue&, const QJsonValue&)>>& contextFilters;
            QVector<std::function<bool (const QJsonValue&)>>& filters;
            int               currentBracketGroupId;

            BracketSink(QVector<Token>& tokens, KeyBuilder& keyBuilder, 
                       QVector<std::function<bool (const QJsonValue&, const QJsonValue&)>>& ctxFilters, QVector<std::function<bool (const QJsonValue&)>>& filterFns, 
                       int bracketGroupId)
                : tk(tokens), kb(keyBuilder), contextFilters(ctxFilters), 
                  filters(filterFns), currentBracketGroupId(bracketGroupId) {}

            // Token emission methods
            std::expected<void, Error> key(QString key, bool allow = false);
            void keyList(const QVector<QString>& keys);
            void wild();
            void slice(const Slice& s);
            void index(int i);
            void pushFilter(const Token& t);
        };

        // ──────────────────────────────────────────────────────────────────────
        //  TableGen-Inspired Rule System Types
        // ──────────────────────────────────────────────────────────────────────

        // Function type aliases for rule system
        // Note: These are used as function pointers in static arrays, so std::function works better
        using BracketRuleMatcher = std::function<bool(QStringView)>;
        using BracketRuleHandler = std::function<std::expected<void, Error>(QStringView, BracketSink&)>;

        /**
         * Declarative rule metadata structure
         */
        struct BracketRuleMetadata {
            const char* name;               // Human-readable rule name
            int priority;                   // Higher priority = checked first
            BracketRuleMatcher matcher;     // Pattern detection function
            BracketRuleHandler handler;     // Processing function
            const char* description;        // Documentation string
        };

        // ──────────────────────────────────────────────────────────────────────
        //  Rule Matcher Functions
        // ──────────────────────────────────────────────────────────────────────

        namespace matchers {
            bool matchesUnionComma(QStringView content);
            bool matchesWildcard(QStringView content);
            bool matchesSingleIndex(QStringView content);
            bool matchesIndexList(QStringView content);
            bool matchesSlice(QStringView content);
            bool matchesFilterWithParens(QStringView content);
            bool matchesFilterWithoutParens(QStringView content);
            bool matchesPlaceholder(QStringView content);
            bool matchesQuotedKey(QStringView content);
            bool matchesUnquotedKey(QStringView content);
        }

        // ──────────────────────────────────────────────────────────────────────
        //  Rule Handler Functions
        // ──────────────────────────────────────────────────────────────────────

        namespace handlers {
            std::expected<void, Error> handleUnionComma(QStringView content, BracketSink& out);
            std::expected<void, Error> handleWildcard(QStringView content, BracketSink& out);
            std::expected<void, Error> handleSingleIndex(QStringView content, BracketSink& out);
            std::expected<void, Error> handleIndexList(QStringView content, BracketSink& out);
            std::expected<void, Error> handleSlice(QStringView content, BracketSink& out);
            std::expected<void, Error> handleFilterWithParens(QStringView content, BracketSink& out);
            std::expected<void, Error> handleFilterWithoutParens(QStringView content, BracketSink& out);
            std::expected<void, Error> handlePlaceholder(QStringView content, BracketSink& out);
            std::expected<void, Error> handleQuotedKey(QStringView content, BracketSink& out);
            std::expected<void, Error> handleUnquotedKey(QStringView content, BracketSink& out);
        }

        // ──────────────────────────────────────────────────────────────────────
        //  TableGen Rule Dispatcher
        // ──────────────────────────────────────────────────────────────────────

        /**
         * Declarative rule dispatcher using TableGen-inspired metadata
         */
        class BracketRuleDispatcher {
        private:
            // Initialize rules at runtime to avoid constexpr issues
            static std::vector<BracketRuleMetadata> createRules();

        public:
            // Get rules (initialized once)
            static const std::vector<BracketRuleMetadata>& getRules();

            // Main dispatch function using declarative rule table
            static std::expected<void, Error> dispatch(QStringView content, BracketSink& sink);

            // Helper for union processing to avoid recursion
            static std::expected<void, Error> processSegmentExcludingUnion(QStringView content, BracketSink& sink);

            // Utility function to get rule metadata for debugging/documentation
            static const BracketRuleMetadata* findRuleByName(const char* name);
        };

    } // namespace detail

} // namespace json_query::json_path
