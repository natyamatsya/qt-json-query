# ADR-005: Versioned Inline ABI Namespace and Hidden Visibility

- **Status:** Accepted
- **Date:** 2026-07-03
- **Context:** qt-json-query is a utility library; multiple libraries inside
  one application may each link it statically

## Problem

When two shared libraries each embed the static `libjson_query.a` and are
loaded into the same process:

- On ELF/Linux with default visibility, both `.so`s export the json_query
  symbols and the dynamic linker *interposes*: all calls resolve to whichever
  library loaded first. With mismatched embedded versions this is an ODR
  violation with silently mixed code and data.
- Version skew (LibA embeds 0.4, LibB embeds 0.6) is the classic utility
  library "diamond problem".

## Decision

Adopt the two standard mitigations used by nlohmann_json (≥3.11,
`nlohmann::json_abi_v3_11_x`), fmt (`fmt::v11`), and Abseil
(`ABSL_OPTION_USE_INLINE_NAMESPACE`):

1. **Versioned inline ABI namespace.** All json_query namespaces are declared
   as `namespace json_query::inline JSON_QUERY_ABI_NS::...` where
   `JSON_QUERY_ABI_NS` (defined in `config/AbiNamespace.hpp`) tracks the
   `<major>_<minor>` version, e.g. `v0_4`. Source code is unaffected (inline
   namespaces are transparent: `json_query::JSONPath` keeps working), but
   mangled symbols carry the tag (`json_query::v0_4::...`), so different
   embedded versions cannot collide or interpose each other.

2. **Hidden visibility on the static library**
   (`CXX_VISIBILITY_PRESET hidden`, `VISIBILITY_INLINES_HIDDEN`): a shared
   library embedding json_query does not re-export its symbols at all.

## Consequences

- Each shared library embedding json_query carries its own copy, including
  its own `thread_local` scratch buffers and `ArrayPool`. These are caches,
  not semantic state — duplication is benign.
- The ABI tag must be bumped together with the project `<major>.<minor>`
  version (release checklist in `AbiNamespace.hpp`). This matches the CMake
  package's `SameMinorVersion` compatibility policy.
- Any pre-namespace binaries are ABI-incompatible; introduced in 0.4.0.
- Forward declarations of json_query entities outside the library must use
  the same inline-namespace declaration; in practice consumers should include
  the public headers instead of forward-declaring.
