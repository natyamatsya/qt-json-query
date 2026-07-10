// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

// KnownFailures.hpp — expected-failure table for the json-patch-tests
// compliance suite (compliance/json-patch-tests).
//
// Semantics (same self-cleaning contract as the IETF JSON Schema suite):
//   mismatch listed here      -> GTEST_SKIP (tracked, justified gap)
//   mismatch not listed here  -> test failure (regression)
//   listed entry that PASSES  -> test failure (stale entry; remove it)
//
// Entries are keyed by (file, index-in-file, comment) — the suite has no
// stable test names, so the comment is included to keep entries readable and
// to guard against silent index shifts when the submodule is bumped: an
// index/comment mismatch makes the entry non-matching, which surfaces as a
// regular failure instead of a silently mis-skipped test.

#include <QString>
#include <array>
#include <cstddef>

namespace rfc6902_compliance
{

struct KnownFailure
{
    const char* file;    // "tests.json" or "spec_tests.json"
    int         index;   // zero-based index in the file's top-level array
    const char* comment; // the case's "comment" member (empty string if none)
};

// Currently empty: the implementation passes the complete suite.
inline constexpr std::array<KnownFailure, 0> kKnownFailures{};

template <std::size_t N>
[[nodiscard]] inline bool matchesKnownFailure(const std::array<KnownFailure, N>& table,
                                              const QString&                     file,
                                              int                                index,
                                              const QString&                     comment) noexcept
{
    for (const auto& kf : table)
    {
        if (file == QLatin1String(kf.file) && index == kf.index && comment == QLatin1String(kf.comment))
            return true;
    }
    return false;
}

} // namespace rfc6902_compliance
