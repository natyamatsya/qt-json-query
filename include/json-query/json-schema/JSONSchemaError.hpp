// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QString>
#include <cstdint>
#include <string_view>

#include "../utils/detail/ErrorMap.hpp"

namespace json_query::json_schema
{

// Schema parse-time errors (occur during schema compilation)
enum class ParseError : std::uint8_t
{
    InvalidSchemaStructure, // Schema must be object or boolean
    InvalidKeywordValue,    // e.g., "type": 123 instead of string/array
    InvalidRegexPattern,    // pattern keyword has invalid regex
    CircularReference,      // $ref creates infinite loop
    UnresolvedReference,    // $ref target not found
    InvalidJsonPointer,     // $ref fragment is invalid JSON Pointer
    UnsupportedDialect,     // $schema specifies unsupported draft
    DuplicateAnchor,        // Same $anchor defined twice
    InvalidTypeValue,       // type keyword has invalid value
    InvalidEnumValue,       // enum must be non-empty array
    EmptySchema,            // Schema document is empty
};

// Schema evaluation-time errors (occur during instance validation)
enum class EvalError : std::uint8_t
{
    TypeMismatch,
    RequiredMissing,
    AdditionalPropertiesInvalid,
    PatternMismatch,
    MinLengthViolation,
    MaxLengthViolation,
    MinimumViolation,
    MaximumViolation,
    ExclusiveMinimumViolation,
    ExclusiveMaximumViolation,
    MultipleOfViolation,
    MinItemsViolation,
    MaxItemsViolation,
    UniqueItemsViolation,
    MinPropertiesViolation,
    MaxPropertiesViolation,
    EnumMismatch,
    ConstMismatch,
    AllOfFailed,
    AnyOfFailed,
    OneOfFailed,
    NotFailed,
    IfThenElseFailed,
    FormatInvalid,         ///< Value does not match format pattern
    FormatSemanticInvalid, ///< Value matches pattern but fails semantic validation (e.g., Feb 30)
    ContentEncodingInvalid,
    UnevaluatedPropertiesInvalid,
    UnevaluatedItemsInvalid,
    DependentRequiredMissing,
    DependentSchemasFailed,
    ContainsViolation,
    PropertyNameInvalid,
};

// JSON Schema parse error messages
inline constexpr auto json_schema_parse_errors = utils::detail::ErrorMap<ParseError, 11>{
    {ParseError::InvalidSchemaStructure, DEFINE_ERROR_STRING("Schema must be a JSON object or boolean")},
    {ParseError::InvalidKeywordValue, DEFINE_ERROR_STRING("Invalid value for schema keyword")},
    {ParseError::InvalidRegexPattern, DEFINE_ERROR_STRING("Invalid regular expression in pattern keyword")},
    {ParseError::CircularReference, DEFINE_ERROR_STRING("Circular reference detected in schema")},
    {ParseError::UnresolvedReference, DEFINE_ERROR_STRING("Unable to resolve $ref target")},
    {ParseError::InvalidJsonPointer, DEFINE_ERROR_STRING("Invalid JSON Pointer in $ref fragment")},
    {ParseError::UnsupportedDialect, DEFINE_ERROR_STRING("Unsupported JSON Schema dialect")},
    {ParseError::DuplicateAnchor, DEFINE_ERROR_STRING("Duplicate $anchor definition")},
    {ParseError::InvalidTypeValue, DEFINE_ERROR_STRING("Invalid value for type keyword")},
    {ParseError::InvalidEnumValue, DEFINE_ERROR_STRING("enum must be a non-empty array")},
    {ParseError::EmptySchema, DEFINE_ERROR_STRING("Schema document is empty")}};

// JSON Schema evaluation error messages
inline constexpr auto json_schema_eval_errors = utils::detail::ErrorMap<EvalError, 32>{
    {EvalError::TypeMismatch, DEFINE_ERROR_STRING("Value does not match expected type")},
    {EvalError::RequiredMissing, DEFINE_ERROR_STRING("Required property is missing")},
    {EvalError::AdditionalPropertiesInvalid, DEFINE_ERROR_STRING("Additional properties are not allowed")},
    {EvalError::PatternMismatch, DEFINE_ERROR_STRING("String does not match required pattern")},
    {EvalError::MinLengthViolation, DEFINE_ERROR_STRING("String is shorter than minimum length")},
    {EvalError::MaxLengthViolation, DEFINE_ERROR_STRING("String exceeds maximum length")},
    {EvalError::MinimumViolation, DEFINE_ERROR_STRING("Value is less than minimum")},
    {EvalError::MaximumViolation, DEFINE_ERROR_STRING("Value exceeds maximum")},
    {EvalError::ExclusiveMinimumViolation, DEFINE_ERROR_STRING("Value must be greater than exclusive minimum")},
    {EvalError::ExclusiveMaximumViolation, DEFINE_ERROR_STRING("Value must be less than exclusive maximum")},
    {EvalError::MultipleOfViolation, DEFINE_ERROR_STRING("Value is not a multiple of the specified number")},
    {EvalError::MinItemsViolation, DEFINE_ERROR_STRING("Array has fewer items than minimum")},
    {EvalError::MaxItemsViolation, DEFINE_ERROR_STRING("Array has more items than maximum")},
    {EvalError::UniqueItemsViolation, DEFINE_ERROR_STRING("Array items are not unique")},
    {EvalError::MinPropertiesViolation, DEFINE_ERROR_STRING("Object has fewer properties than minimum")},
    {EvalError::MaxPropertiesViolation, DEFINE_ERROR_STRING("Object has more properties than maximum")},
    {EvalError::EnumMismatch, DEFINE_ERROR_STRING("Value is not one of the allowed enum values")},
    {EvalError::ConstMismatch, DEFINE_ERROR_STRING("Value does not match const")},
    {EvalError::AllOfFailed, DEFINE_ERROR_STRING("Value does not match all schemas in allOf")},
    {EvalError::AnyOfFailed, DEFINE_ERROR_STRING("Value does not match any schema in anyOf")},
    {EvalError::OneOfFailed, DEFINE_ERROR_STRING("Value does not match exactly one schema in oneOf")},
    {EvalError::NotFailed, DEFINE_ERROR_STRING("Value matches schema in not")},
    {EvalError::IfThenElseFailed, DEFINE_ERROR_STRING("Value does not satisfy if/then/else condition")},
    {EvalError::FormatInvalid, DEFINE_ERROR_STRING("Value does not match required format")},
    {EvalError::FormatSemanticInvalid, DEFINE_ERROR_STRING("Value matches format but is semantically invalid")},
    {EvalError::ContentEncodingInvalid, DEFINE_ERROR_STRING("Invalid content encoding")},
    {EvalError::UnevaluatedPropertiesInvalid, DEFINE_ERROR_STRING("Unevaluated properties are not allowed")},
    {EvalError::UnevaluatedItemsInvalid, DEFINE_ERROR_STRING("Unevaluated items are not allowed")},
    {EvalError::DependentRequiredMissing, DEFINE_ERROR_STRING("Dependent required property is missing")},
    {EvalError::DependentSchemasFailed, DEFINE_ERROR_STRING("Dependent schema validation failed")},
    {EvalError::ContainsViolation, DEFINE_ERROR_STRING("Array does not contain required item")},
    {EvalError::PropertyNameInvalid, DEFINE_ERROR_STRING("Property name does not match schema")}};

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

} // namespace json_query::json_schema
