// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Machine-readable known-failure (xfail) table for the IETF JSON Schema
// Draft 2020-12 compliance suite.
//
// Every entry is an exact (file, group, test) triple that is EXPECTED to
// mismatch in the current build configuration. The driver skips entries in
// force and fails hard when an entry unexpectedly passes, so this table can
// never mask a fixed test or a new regression:
//   - mismatch listed here      -> GTEST_SKIP (known optional-feature gap)
//   - mismatch not listed here  -> test failure (regression)
//   - listed entry that passes  -> test failure (stale entry; remove it)
//
// Buckets are selected by build features (see IETFTests.cmake):
//   kKnownFailuresNoEcmaRegex  - resolved by JSON_QUERY_FORMAT_ECMA_REGEX (SRELL)
//   kKnownFailuresNoIdn        - resolved by JSON_QUERY_FORMAT_IDN (libidn2/ada)
//   kKnownFailuresHostname     - strict-ASCII hostname validator gaps (always active)

#include <QString>

#include <cstddef>

struct KnownFailure
{
    const char* file;
    const char* group;
    const char* test;
};

// ECMA-262 regex semantics (Unicode classes, \d/\w ASCII-only, etc.) --------
inline constexpr KnownFailure kKnownFailuresNoEcmaRegex[] = {
    {"ecmascript-regex.json", "ECMA 262 \\S matches everything but whitespace", "EM SPACE does not match (Space_Separator)"},
    {"ecmascript-regex.json", "ECMA 262 \\S matches everything but whitespace", "latin-1 non-breaking-space does not match"},
    {"ecmascript-regex.json", "ECMA 262 \\S matches everything but whitespace", "paragraph separator does not match (line terminator)"},
    {"ecmascript-regex.json", "ECMA 262 \\S matches everything but whitespace", "zero-width whitespace does not match"},
    {"ecmascript-regex.json", "ECMA 262 \\s matches whitespace", "EM SPACE matches (Space_Separator)"},
    {"ecmascript-regex.json", "ECMA 262 \\s matches whitespace", "latin-1 non-breaking-space matches"},
    {"ecmascript-regex.json", "ECMA 262 \\s matches whitespace", "paragraph separator matches (line terminator)"},
    {"ecmascript-regex.json", "ECMA 262 \\s matches whitespace", "zero-width whitespace matches"},
    {"ecmascript-regex.json", "\\a is not an ECMA 262 control escape", "when used as a pattern"},
    {"ecmascript-regex.json", "pattern with non-ASCII digits", "ascii digits"},
    {"ecmascript-regex.json", "pattern with non-ASCII digits", "ascii non-digits"},
    {"ecmascript-regex.json", "pattern with non-ASCII digits", "non-ascii digits (BENGALI DIGIT FOUR, BENGALI DIGIT TWO)"},
    {"ecmascript-regex.json", "patternProperties with non-ASCII digits", "ascii digits"},
    {"ecmascript-regex.json", "patternProperties with non-ASCII digits", "ascii non-digits"},
    {"ecmascript-regex.json", "patternProperties with non-ASCII digits", "non-ascii digits (BENGALI DIGIT FOUR, BENGALI DIGIT TWO)"},
    {"ecmascript-regex.json", "patterns always use unicode semantics with pattern", "ascii character in json string"},
    {"ecmascript-regex.json", "patterns always use unicode semantics with pattern", "literal unicode character in json string"},
    {"ecmascript-regex.json", "patterns always use unicode semantics with pattern", "unicode character in hex format in string"},
    {"ecmascript-regex.json", "patterns always use unicode semantics with pattern", "unicode matching is case-sensitive"},
    {"ecmascript-regex.json", "patterns always use unicode semantics with patternProperties", "ascii character in json string"},
    {"ecmascript-regex.json", "patterns always use unicode semantics with patternProperties", "literal unicode character in json string"},
    {"ecmascript-regex.json", "patterns always use unicode semantics with patternProperties", "unicode character in hex format in string"},
    {"ecmascript-regex.json", "patterns always use unicode semantics with patternProperties", "unicode matching is case-sensitive"},
};

// IDN hostname/email (IDNA 2008 contextual rules) ----------------------------
inline constexpr KnownFailure kKnownFailuresNoIdn[] = {
    {"idn-email.json", "validation of an internationalized e-mail addresses", "a valid idn e-mail (example@example.test in Hangul)"},
    {"idn-hostname.json", "validation of internationalized host names", "Arabic-Indic digits not mixed with Extended Arabic-Indic digits"},
    {"idn-hostname.json", "validation of internationalized host names", "Exceptions that are PVALID, left-to-right chars"},
    {"idn-hostname.json", "validation of internationalized host names", "Exceptions that are PVALID, right-to-left chars"},
    {"idn-hostname.json", "validation of internationalized host names", "Extended Arabic-Indic digits not mixed with Arabic-Indic digits"},
    {"idn-hostname.json", "validation of internationalized host names", "Greek KERAIA followed by Greek"},
    {"idn-hostname.json", "validation of internationalized host names", "Hebrew GERESH preceded by Hebrew"},
    {"idn-hostname.json", "validation of internationalized host names", "Hebrew GERSHAYIM preceded by Hebrew"},
    {"idn-hostname.json", "validation of internationalized host names", "KATAKANA MIDDLE DOT with Han"},
    {"idn-hostname.json", "validation of internationalized host names", "KATAKANA MIDDLE DOT with Hiragana"},
    {"idn-hostname.json", "validation of internationalized host names", "KATAKANA MIDDLE DOT with Katakana"},
    {"idn-hostname.json", "validation of internationalized host names", "MIDDLE DOT with surrounding 'l's"},
    {"idn-hostname.json", "validation of internationalized host names", "ZERO WIDTH JOINER preceded by Virama"},
    {"idn-hostname.json", "validation of internationalized host names", "ZERO WIDTH NON-JOINER not preceded by Virama but matches regexp"},
    {"idn-hostname.json", "validation of internationalized host names", "ZERO WIDTH NON-JOINER preceded by Virama"},
    {"idn-hostname.json", "validation of internationalized host names", "a valid host name (example.test in Hangul)"},
    {"idn-hostname.json", "validation of internationalized host names", "valid Chinese Punycode"},
    {"idn-hostname.json", "validation of separators in internationalized host names", "fullwidth full stop as label separator"},
    {"idn-hostname.json", "validation of separators in internationalized host names", "halfwidth ideographic full stop as label separator"},
    {"idn-hostname.json", "validation of separators in internationalized host names", "ideographic full stop as label separator"},
    {"idn-hostname.json", "validation of separators in internationalized host names", "label too long if separator ignored (full stop)"},
    {"idn-hostname.json", "validation of separators in internationalized host names", "label too long if separator ignored (fullwidth full stop)"},
    {"idn-hostname.json", "validation of separators in internationalized host names", "label too long if separator ignored (halfwidth ideographic full stop)"},
    {"idn-hostname.json", "validation of separators in internationalized host names", "label too long if separator ignored (ideographic full stop)"},
};

// Unicode hostnames rejected by the strict-ASCII RFC 1123 validator ----------
inline constexpr KnownFailure kKnownFailuresHostname[] = {
    {"hostname.json", "validation of A-label (punycode) host names", "Arabic-Indic digits not mixed with Extended Arabic-Indic digits"},
    {"hostname.json", "validation of A-label (punycode) host names", "Exceptions that are PVALID, left-to-right chars"},
    {"hostname.json", "validation of A-label (punycode) host names", "Exceptions that are PVALID, right-to-left chars"},
    {"hostname.json", "validation of A-label (punycode) host names", "Extended Arabic-Indic digits not mixed with Arabic-Indic digits"},
    {"hostname.json", "validation of A-label (punycode) host names", "Greek KERAIA followed by Greek"},
    {"hostname.json", "validation of A-label (punycode) host names", "Hebrew GERESH preceded by Hebrew"},
    {"hostname.json", "validation of A-label (punycode) host names", "Hebrew GERSHAYIM preceded by Hebrew"},
    {"hostname.json", "validation of A-label (punycode) host names", "KATAKANA MIDDLE DOT with Han"},
    {"hostname.json", "validation of A-label (punycode) host names", "KATAKANA MIDDLE DOT with Hiragana"},
    {"hostname.json", "validation of A-label (punycode) host names", "KATAKANA MIDDLE DOT with Katakana"},
    {"hostname.json", "validation of A-label (punycode) host names", "MIDDLE DOT with surrounding 'l's"},
    {"hostname.json", "validation of A-label (punycode) host names", "ZERO WIDTH JOINER preceded by Virama"},
    {"hostname.json", "validation of A-label (punycode) host names", "ZERO WIDTH NON-JOINER not preceded by Virama but matches regexp"},
    {"hostname.json", "validation of A-label (punycode) host names", "ZERO WIDTH NON-JOINER preceded by Virama"},
    {"hostname.json", "validation of A-label (punycode) host names", "a valid host name (example.test in Hangul)"},
};

template <std::size_t N>
inline bool matchesKnownFailure(const KnownFailure (&table)[N],
                                const QString& file,
                                const QString& group,
                                const QString& test)
{
    for (const auto& kf : table)
    {
        if (file == QLatin1StringView{kf.file} && group == QLatin1StringView{kf.group} &&
            test == QLatin1StringView{kf.test})
            return true;
    }
    return false;
}
