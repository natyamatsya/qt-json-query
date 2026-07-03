// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "json-query/json-path/JSONPathCompile.hpp"
#include <QString>
#include <optional>

namespace json_query::json_path::detail
{

// Import types from the main namespace
using Token    = json_query::json_path::Token;
using FilterFn = json_query::json_path::FilterFn;

// ============================================================================
// TableGen-Inspired Existence Pattern Dispatch System
// ============================================================================

/**
 * @brief Existence pattern types for compile-time dispatch
 *
 * Each type represents a distinct existence checking pattern with specific
 * regex matching and evaluation logic.
 */
enum class ExistencePatternType : uint8_t
{
    NestedFilter,     // @[?@>1] - Nested filter patterns (highest priority)
    SimpleContext,    // @ - Simple relative context existence
    SimpleRoot,       // $ - Simple absolute root existence
    Wildcard,         // @.* - Wildcard existence pattern
    Slice,            // @[1:3] - Slice existence pattern
    MultiSelector,    // @[0,1,'key'] - Multiple selector existence
    AbsoluteWildcard, // $.* - Absolute wildcard existence
    AbsoluteComplex,  // $.*.property - Complex absolute path
    AbsoluteDot,      // $.property - Root property existence
    BasicDot,         // @.property - Context property existence
    BracketProperty,  // @['property'] - Bracket notation property
    IndexPattern      // @[0] - Array index existence
};

/**
 * @brief TableGen-style existence pattern definitions
 *
 * Each specialization defines the characteristics, priority, and matching
 * logic for a specific existence pattern type.
 */
template <ExistencePatternType PatternType>
struct ExistencePatternDef
{
    static constexpr bool enabled  = false;
    static constexpr int  priority = 0;
    static bool           matches(const QString& s) { return false; }
};

/**
 * @brief Template specialization strategies for existence pattern processing
 *
 * Each strategy implements the specific token creation and filter logic
 * for its corresponding existence pattern type.
 */
template <ExistencePatternType PatternType>
struct ExistencePatternStrategy
{
    static std::optional<Token> process(const QString& s)
    {
        // Default implementation should never be called
        return std::nullopt;
    }
};

/**
 * @brief TableGen-inspired recursive dispatch table for existence patterns
 *
 * Uses variadic templates for priority-ordered dispatch through all
 * existence pattern strategies until a match is found.
 */
template <ExistencePatternType... Types>
struct ExistencePatternDispatchTable;

template <ExistencePatternType FirstType, ExistencePatternType... RestTypes>
struct ExistencePatternDispatchTable<FirstType, RestTypes...>
{
    static std::optional<Token> dispatch(const QString& s)
    {
        // Try the first strategy
        if constexpr (ExistencePatternDef<FirstType>::enabled)
        {
            if (ExistencePatternDef<FirstType>::matches(s))
                return ExistencePatternStrategy<FirstType>::process(s);
        }

        // Recurse to remaining types
        if constexpr (sizeof...(RestTypes) > 0)
        {
            return ExistencePatternDispatchTable<RestTypes...>::dispatch(s);
        }
        else
        {
            // No more strategies to try
            return std::nullopt;
        }
    }
};

// Base case: single type
template <ExistencePatternType FirstType>
struct ExistencePatternDispatchTable<FirstType>
{
    static std::optional<Token> dispatch(const QString& s)
    {
        if constexpr (ExistencePatternDef<FirstType>::enabled)
        {
            if (ExistencePatternDef<FirstType>::matches(s))
                return ExistencePatternStrategy<FirstType>::process(s);
        }
        return std::nullopt;
    }
};

/**
 * @brief Main existence pattern dispatcher
 *
 * Defines the priority-ordered list of existence pattern strategies
 * and provides the main dispatch entry point.
 */
class ExistencePatternDispatcher
{
  public:
    // Priority-ordered strategy list (highest priority first)
    using DispatchTable =
        ExistencePatternDispatchTable<ExistencePatternType::NestedFilter,  // Priority 1200 - Most specific
                                      ExistencePatternType::SimpleContext, // Priority 1100 - Simple patterns
                                      ExistencePatternType::SimpleRoot,    // Priority 1100 - Simple patterns
                                      ExistencePatternType::Wildcard,      // Priority 1000 - Wildcard patterns
                                      ExistencePatternType::Slice,         // Priority 900  - Slice patterns
                                      ExistencePatternType::MultiSelector, // Priority 800  - Multi-selector patterns
                                      ExistencePatternType::AbsoluteWildcard, // Priority 700  - Absolute wildcards
                                      ExistencePatternType::AbsoluteComplex,  // Priority 600  - Complex absolute paths
                                      ExistencePatternType::AbsoluteDot,      // Priority 500  - Absolute properties
                                      ExistencePatternType::BasicDot,         // Priority 400  - Basic properties
                                      ExistencePatternType::BracketProperty,  // Priority 300  - Bracket properties
                                      ExistencePatternType::IndexPattern      // Priority 200  - Index patterns
                                      >;

    static std::optional<Token> dispatch(const QString& s);
};

/**
 * @brief Main entry point for existence pattern parsing
 *
 * Processes a string expression and returns a Token if it matches
 * any of the supported existence patterns.
 */
std::optional<Token> parseEmbeddedExists(const QString& s);

} // namespace json_query::json_path::detail
