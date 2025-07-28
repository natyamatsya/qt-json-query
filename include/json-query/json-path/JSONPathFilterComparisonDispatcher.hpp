#pragma once

#include <QString>
#include <QJsonValue>
#include <optional>
#include <vector>
#include <ctre.hpp>

#include "JSONPathFilterFunctions.hpp"
#include "JSONPathFilterComparison.hpp"
#include "JSONPathCompile.hpp"
#include "JSONPathFilterHelpers.hpp"

namespace json_query::json_path::detail
{

// Use Token and FilterFn from parent namespace, Builder from current namespace
using json_query::json_path::FilterFn;
using json_query::json_path::Token;
// Builder is already in detail namespace, no using needed

// ============================================================================
// TableGen-Inspired Comparison Dispatcher Infrastructure
// ============================================================================

// Enum representing all comparison filter types with priority ordering
enum class ComparisonFilterType
{
    // Null comparisons (highest priority - most specific)
    NullPropertyDot,     // @.prop == null
    NullPropertyBracket, // @["key"] == null
    NullArrayIndex,      // @[index] == null

    // Self comparisons (high priority)
    DirectSelf,          // @ == @
    SelfPropertyDot,     // @.prop == @
    SelfPropertyBracket, // @["key"] == @
    SelfArrayIndex,      // @[index] == @
    SelfValue,           // @ == value

    // Property-to-property comparisons (medium priority)
    PropertyToProperty, // @.a == @.b
    PropertyToArray,    // @.prop == @.list[9]
    ArrayToProperty,    // @.list[9] == @.prop

    // Basic property comparisons (lowest priority - most general)
    BasicPropertyDot,     // @.prop == value
    BasicPropertyBracket, // @["key"] == value
    BasicArrayIndex       // @[index] == value
};

// Pattern definition template specializations (TableGen-style record definitions)
template <ComparisonFilterType Type>
struct ComparisonPatternDef
{
    static constexpr bool               enabled = false;
    static constexpr ctll::fixed_string pattern{""};
};

// Token factory template specializations (TableGen-style code generation)
template <ComparisonFilterType Type>
struct ComparisonTokenFactory
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        return std::nullopt; // Default: not implemented
    }
};

// Variadic template dispatch table (TableGen-inspired compile-time dispatch)
template <ComparisonFilterType... Types>
struct ComparisonDispatchTable
{
    static std::optional<Token> dispatch(const QString& s, std::vector<FilterFn>& out);

  private:
    template <ComparisonFilterType First, ComparisonFilterType... Rest>
    static std::optional<Token> dispatchImpl(const QString& s, std::vector<FilterFn>& out);
};

// Complete dispatch table with priority ordering (most specific first)
using ComparisonDispatcher = ComparisonDispatchTable<
    // Null comparisons (highest priority)
    ComparisonFilterType::NullPropertyDot,
    ComparisonFilterType::NullPropertyBracket,
    ComparisonFilterType::NullArrayIndex,

    // Self comparisons (high priority)
    ComparisonFilterType::DirectSelf,
    ComparisonFilterType::SelfPropertyDot,
    ComparisonFilterType::SelfPropertyBracket,
    ComparisonFilterType::SelfArrayIndex,
    ComparisonFilterType::SelfValue,

    // Property-to-property comparisons (medium priority)
    ComparisonFilterType::PropertyToProperty,
    ComparisonFilterType::PropertyToArray,
    ComparisonFilterType::ArrayToProperty,

    // Basic property comparisons (lowest priority)
    ComparisonFilterType::BasicPropertyDot,
    ComparisonFilterType::BasicPropertyBracket,
    ComparisonFilterType::BasicArrayIndex>;

} // namespace json_query::json_path::detail
