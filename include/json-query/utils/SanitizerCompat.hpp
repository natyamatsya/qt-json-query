// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

/**
 * @file SanitizerCompat.hpp
 * @brief Sanitizer compatibility macros for Qt framework integration
 *
 * This header provides macros to selectively exclude functions from sanitizer
 * instrumentation when they conflict with Qt's internal memory management.
 *
 * See doc/SanitizerCompatibility.md for detailed analysis and rationale.
 */

/**
 * @def JSON_QUERY_NO_SANITIZE_QT_COMPAT
 * @brief Excludes function from sanitizer instrumentation due to Qt compatibility issues
 *
 * Use this macro on functions that interact with Qt's copy-on-write semantics
 * (QJsonArray, QJsonObject, QJsonValue) and experience functional failures
 * under AddressSanitizer instrumentation.
 *
 * This does NOT indicate memory safety issues - it addresses instrumentation
 * interference with Qt's internal optimizations.
 *
 * Example:
 * @code
 * JSON_QUERY_NO_SANITIZE_QT_COMPAT
 * bool processQtData(const QJsonArray& arr) {
 *     // Qt operations that fail under sanitizer instrumentation
 * }
 * @endcode
 */
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define JSON_QUERY_NO_SANITIZE_QT_COMPAT __attribute__((no_sanitize("address")))
#else
#define JSON_QUERY_NO_SANITIZE_QT_COMPAT
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define JSON_QUERY_NO_SANITIZE_QT_COMPAT __attribute__((no_sanitize_address))
#else
#define JSON_QUERY_NO_SANITIZE_QT_COMPAT
#endif

/**
 * @def JSON_QUERY_SANITIZER_DIAGNOSTIC
 * @brief Compile-time diagnostic for sanitizer exclusions
 *
 * Emits a diagnostic message when sanitizer exclusions are active,
 * helping developers understand when and why functions are excluded.
 */
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define JSON_QUERY_SANITIZER_DIAGNOSTIC(msg) _Pragma("message(\"JSON Query: Sanitizer exclusion active - \" msg)")
#else
#define JSON_QUERY_SANITIZER_DIAGNOSTIC(msg)
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define JSON_QUERY_SANITIZER_DIAGNOSTIC(msg) _Pragma("message(\"JSON Query: Sanitizer exclusion active - \" msg)")
#else
#define JSON_QUERY_SANITIZER_DIAGNOSTIC(msg)
#endif
