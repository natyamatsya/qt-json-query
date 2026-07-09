// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include <QtCore/QString>
#include <cstdint>
#include <string_view>

#include "../utils/detail/ErrorMap.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_schema
{

// Schema parse-time errors (occur during schema compilation).
// Grouped by category, alphabetical within each group (ADR-004: numeric
// values are not API, so entries are freely sortable).
enum class ParseError : std::uint8_t
{
    // --- Schema structure ---
    EmptySchema,            // Schema document is empty
    InvalidSchemaStructure, // Schema must be object or boolean

    // --- Keyword values ---
    InvalidEnumValue,    // enum must be non-empty array
    InvalidKeywordValue, // e.g., "type": 123 instead of string/array
    InvalidRegexPattern, // pattern keyword has invalid regex
    InvalidTypeValue,    // type keyword has invalid value

    // --- References and anchors ---
    CircularReference,   // $ref creates infinite loop
    DuplicateAnchor,     // Same $anchor defined twice
    InvalidJsonPointer,  // $ref fragment is invalid JSON Pointer
    UnresolvedReference, // $ref target not found

    // --- Dialect ---
    UnsupportedDialect, // $schema specifies unsupported draft
};

// Schema evaluation-time errors (occur during instance validation).
// Grouped by JSON Schema keyword family, alphabetical within each group
// (ADR-004: numeric values are not API, so entries are freely sortable).
enum class EvalError : std::uint8_t
{
    // --- Any type: type / const / enum ---
    ConstMismatch,
    EnumMismatch,
    TypeMismatch,
    // --- String keywords ---
    MaxLengthViolation,
    MinLengthViolation,
    PatternMismatch,
    // --- Numeric keywords ---
    ExclusiveMaximumViolation,
    ExclusiveMinimumViolation,
    MaximumViolation,
    MinimumViolation,
    MultipleOfViolation,
    // --- Array keywords ---
    ContainsViolation,
    MaxItemsViolation,
    MinItemsViolation,
    UniqueItemsViolation,
    // --- Object keywords ---
    AdditionalPropertiesInvalid,
    MaxPropertiesViolation,
    MinPropertiesViolation,
    PropertyNameInvalid,
    RequiredMissing,
    // --- Combinators and conditionals ---
    AllOfFailed,
    AnyOfFailed,
    IfThenElseFailed,
    NotFailed,
    OneOfFailed,
    // --- Dependent keywords ---
    DependentRequiredMissing,
    DependentSchemasFailed,
    // --- Format and content ---
    ContentEncodingInvalid,
    FormatInvalid,         ///< Value does not match format pattern
    FormatSemanticInvalid, ///< Value matches pattern but fails semantic validation (e.g., Feb 30)

    // --- Unevaluated keywords ---
    UnevaluatedItemsInvalid,
    UnevaluatedPropertiesInvalid,

    // --- References ---
    RefCycleDetected, ///< $ref/$dynamicRef cycle that consumes no instance input
};

// JSON Schema parse error messages (same order as the enum)
inline constexpr auto json_schema_parse_errors = utils::detail::ErrorMap<ParseError, 11>{
    {ParseError::EmptySchema, DEFINE_ERROR_STRING("Schema document is empty")},
    {ParseError::InvalidSchemaStructure, DEFINE_ERROR_STRING("Schema must be a JSON object or boolean")},
    {ParseError::InvalidEnumValue, DEFINE_ERROR_STRING("enum must be a non-empty array")},
    {ParseError::InvalidKeywordValue, DEFINE_ERROR_STRING("Invalid value for schema keyword")},
    {ParseError::InvalidRegexPattern, DEFINE_ERROR_STRING("Invalid regular expression in pattern keyword")},
    {ParseError::InvalidTypeValue, DEFINE_ERROR_STRING("Invalid value for type keyword")},
    {ParseError::CircularReference, DEFINE_ERROR_STRING("Circular reference detected in schema")},
    {ParseError::DuplicateAnchor, DEFINE_ERROR_STRING("Duplicate $anchor definition")},
    {ParseError::InvalidJsonPointer, DEFINE_ERROR_STRING("Invalid JSON Pointer in $ref fragment")},
    {ParseError::UnresolvedReference, DEFINE_ERROR_STRING("Unable to resolve $ref target")},
    {ParseError::UnsupportedDialect, DEFINE_ERROR_STRING("Unsupported JSON Schema dialect")}};

// JSON Schema evaluation error messages (same order as the enum)
inline constexpr auto json_schema_eval_errors = utils::detail::ErrorMap<EvalError, 33>{
    {EvalError::ConstMismatch, DEFINE_ERROR_STRING("Value does not match const")},
    {EvalError::EnumMismatch, DEFINE_ERROR_STRING("Value is not one of the allowed enum values")},
    {EvalError::TypeMismatch, DEFINE_ERROR_STRING("Value does not match expected type")},
    {EvalError::MaxLengthViolation, DEFINE_ERROR_STRING("String exceeds maximum length")},
    {EvalError::MinLengthViolation, DEFINE_ERROR_STRING("String is shorter than minimum length")},
    {EvalError::PatternMismatch, DEFINE_ERROR_STRING("String does not match required pattern")},
    {EvalError::ExclusiveMaximumViolation, DEFINE_ERROR_STRING("Value must be less than exclusive maximum")},
    {EvalError::ExclusiveMinimumViolation, DEFINE_ERROR_STRING("Value must be greater than exclusive minimum")},
    {EvalError::MaximumViolation, DEFINE_ERROR_STRING("Value exceeds maximum")},
    {EvalError::MinimumViolation, DEFINE_ERROR_STRING("Value is less than minimum")},
    {EvalError::MultipleOfViolation, DEFINE_ERROR_STRING("Value is not a multiple of the specified number")},
    {EvalError::ContainsViolation, DEFINE_ERROR_STRING("Array does not contain required item")},
    {EvalError::MaxItemsViolation, DEFINE_ERROR_STRING("Array has more items than maximum")},
    {EvalError::MinItemsViolation, DEFINE_ERROR_STRING("Array has fewer items than minimum")},
    {EvalError::UniqueItemsViolation, DEFINE_ERROR_STRING("Array items are not unique")},
    {EvalError::AdditionalPropertiesInvalid, DEFINE_ERROR_STRING("Additional properties are not allowed")},
    {EvalError::MaxPropertiesViolation, DEFINE_ERROR_STRING("Object has more properties than maximum")},
    {EvalError::MinPropertiesViolation, DEFINE_ERROR_STRING("Object has fewer properties than minimum")},
    {EvalError::PropertyNameInvalid, DEFINE_ERROR_STRING("Property name does not match schema")},
    {EvalError::RequiredMissing, DEFINE_ERROR_STRING("Required property is missing")},
    {EvalError::AllOfFailed, DEFINE_ERROR_STRING("Value does not match all schemas in allOf")},
    {EvalError::AnyOfFailed, DEFINE_ERROR_STRING("Value does not match any schema in anyOf")},
    {EvalError::IfThenElseFailed, DEFINE_ERROR_STRING("Value does not satisfy if/then/else condition")},
    {EvalError::NotFailed, DEFINE_ERROR_STRING("Value matches schema in not")},
    {EvalError::OneOfFailed, DEFINE_ERROR_STRING("Value does not match exactly one schema in oneOf")},
    {EvalError::DependentRequiredMissing, DEFINE_ERROR_STRING("Dependent required property is missing")},
    {EvalError::DependentSchemasFailed, DEFINE_ERROR_STRING("Dependent schema validation failed")},
    {EvalError::ContentEncodingInvalid, DEFINE_ERROR_STRING("Invalid content encoding")},
    {EvalError::FormatInvalid, DEFINE_ERROR_STRING("Value does not match required format")},
    {EvalError::FormatSemanticInvalid, DEFINE_ERROR_STRING("Value matches format but is semantically invalid")},
    {EvalError::UnevaluatedItemsInvalid, DEFINE_ERROR_STRING("Unevaluated items are not allowed")},
    {EvalError::UnevaluatedPropertiesInvalid, DEFINE_ERROR_STRING("Unevaluated properties are not allowed")},
    {EvalError::RefCycleDetected, DEFINE_ERROR_STRING("Infinite $ref recursion detected in schema")}};

/**
 * @brief Convert a ParseError to a human-readable string view
 */
[[nodiscard]] constexpr std::string_view to_std_sv(ParseError e) noexcept
{
    return json_schema_parse_errors.get_std_sv(e);
}

/**
 * @brief Convert a ParseError to a QStringView
 */
[[nodiscard]] constexpr QStringView to_qt_sv(ParseError e) noexcept { return json_schema_parse_errors.get_qt_sv(e); }

/**
 * @brief Convert an EvalError to a human-readable string view
 */
[[nodiscard]] constexpr std::string_view to_std_sv(EvalError e) noexcept
{
    return json_schema_eval_errors.get_std_sv(e);
}

/**
 * @brief Convert an EvalError to a QStringView
 */
[[nodiscard]] constexpr QStringView to_qt_sv(EvalError e) noexcept { return json_schema_eval_errors.get_qt_sv(e); }

} // namespace json_query::inline JSON_QUERY_ABI_NS::json_schema
