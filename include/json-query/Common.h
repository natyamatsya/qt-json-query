#pragma once

/**
 * @file Common.h
 * @brief Common macros and definitions for the qt-json-query library
 *
 * This header provides cross-platform macros and utilities that are used
 * throughout the qt-json-query library, particularly for performance-critical
 * code paths that require compiler-specific optimizations.
 */

// Cross-platform always inline macro for performance-critical hot paths
// This macro provides consistent forced inlining across different compilers
// and platforms to eliminate function call overhead in template-heavy code.

#if defined(__GNUC__) || defined(__clang__)
// GCC and Clang support the __attribute__((always_inline)) syntax
// Note: Clang supports both GNU syntax and [[gnu::always_inline]], but
// __attribute__((always_inline)) is more traditional and widely supported
#define QT_QUERY_JSON_ALWAYS_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
// MSVC uses __forceinline
#define QT_QUERY_JSON_ALWAYS_INLINE __forceinline
#elif defined(__INTEL_COMPILER)
// Intel C++ Compiler
#define QT_QUERY_JSON_ALWAYS_INLINE __forceinline
#else
// Fallback for unknown compilers - use standard inline
#define QT_QUERY_JSON_ALWAYS_INLINE inline
#endif

// Additional performance-related macros can be added here as needed

/**
 * @brief Macro for marking functions as likely to be called frequently
 *
 * This can be used to provide hints to the compiler about hot functions
 * that should be optimized for performance over code size.
 */
#if defined(__GNUC__) || defined(__clang__)
#define QT_QUERY_JSON_HOT __attribute__((hot))
#else
#define QT_QUERY_JSON_HOT
#endif

/**
 * @brief Macro for marking functions as unlikely to be called
 *
 * This can be used for error handling paths and other cold code
 * to optimize the hot path layout.
 */
#if defined(__GNUC__) || defined(__clang__)
#define QT_QUERY_JSON_COLD __attribute__((cold))
#else
#define QT_QUERY_JSON_COLD
#endif

/**
 * @brief Macro for branch prediction hints
 */
#if defined(__GNUC__) || defined(__clang__)
#define QT_QUERY_JSON_LIKELY(x) __builtin_expect(!!(x), 1)
#define QT_QUERY_JSON_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define QT_QUERY_JSON_LIKELY(x) (x)
#define QT_QUERY_JSON_UNLIKELY(x) (x)
#endif
