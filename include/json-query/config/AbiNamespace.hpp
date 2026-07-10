// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

/**
 * @file AbiNamespace.hpp
 * @brief Versioned inline ABI namespace tag (the nlohmann_json / fmt pattern).
 *
 * Every json_query namespace is declared as
 *
 *     namespace json_query::inline JSON_QUERY_ABI_NS::... { ... }
 *
 * Source code keeps writing `json_query::JSONPath` (inline namespaces are
 * transparent to lookup), but the *mangled symbol names* carry the version
 * tag. Two libraries that each embed a different json_query version can
 * therefore coexist in one process without ODR violations or dynamic-linker
 * interposition mixing their symbols (see doc/adr/005).
 *
 * RELEASE CHECKLIST: this tag tracks the SameMinorVersion compatibility
 * policy of the CMake package — bump it whenever the project's
 * <major>.<minor> version changes (e.g. project VERSION 0.10.0 -> v0_10).
 */
#define JSON_QUERY_ABI_NS v0_10
