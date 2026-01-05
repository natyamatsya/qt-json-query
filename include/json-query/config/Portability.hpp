// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

/**
 * @brief Portability macros for C++ attributes and compiler-specific features
 */

// [[no_unique_address]] attribute support
// C++20 attribute, but not all compilers support it properly
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(no_unique_address)
#if defined(__clang__)
// Clang warns about this attribute, suppress the warning
#define QT_JSON_QUERY_NO_UNIQUE_ADDRESS                                                           \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wunknown-attributes\"") \
        [[no_unique_address]] _Pragma("clang diagnostic pop")
#else
#define QT_JSON_QUERY_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
#else
#define QT_JSON_QUERY_NO_UNIQUE_ADDRESS
#endif
#else
#define QT_JSON_QUERY_NO_UNIQUE_ADDRESS
#endif
