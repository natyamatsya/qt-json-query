# qt-json-query: extension suggestions from the AC-3033 spike

Findings from integrating [qt-json-query](https://github.com/natyamatsya/qt-json-query)
(pinned: `main@52731b7`, v0.6.0) into ArrayCalc and refactoring the first
consumers. Suggestions are ordered by the value they would unlock in this
codebase.

## Context: what the spike covered

- `libraries/qt-json-query` submodule, wired into the superbuild ahead of its
  consumers (static lib, C++23 confined to the library and consumer `.cpp`s).
- **JSONSchema**: `MilanDeviceEntity::InitializeData` now validates the Milan
  export templates against `Resources/MilanManager/MilanDeviceEntity.schema.json`
  (Draft 2020-12) instead of a single `contains("entity_model")` check.
- **JSONPointer/JSONPath (read)**: `dbVenueDatabase`'s `ConnectorJsonRest`
  parses the Drupal JSON:API replies with compiled pointers (fixed deep
  paths), path filters (`$[?(@.type=='user')]`), and monadic
  `evaluate(...).and_then(as<T>).value_or(default)` chains instead of
  `value().toObject()` chains and `foreach` type loops.
- **JSONPointer (write, v0.6.0)**: the Milan exporter's extract → modify →
  multi-level write-back ladders are gone. `UpdateId`/`UpdateDeviceName` are
  one `set()` each; `UpdateMacAddress` and the connection writers address
  array elements by root-relative pointer paths, use `createIntermediates`
  upserts, and append stream mappings via the RFC 6901 `-` token.

## 1. Writable JSON Pointer — delivered in v0.6.0, adopted

v0.5.0 was read-only; v0.6.0 added `add`/`replace`/`remove`/`set` (RFC 6902
§4 semantics, strong guarantee) plus the JSON Patch and Merge Patch modules.
The spike adopted `set(root, value, {.createIntermediates = true})` throughout
`src/Export/Data/MilanDeviceEntity.cpp` — the file dropped ~140 lines of
write-back plumbing, and a missed manual write-back is no longer a possible
bug there. Remaining candidate: `libraries/dbACWebUpdate/.../Settings/
JsonFileSettingsBackend.cpp`, a bespoke ~120-line recursive RFC-6901-style
read/write engine that can now be replaced wholesale (out of scope for this
spike; the library-side prerequisite is met).

First-consumer observations on the write API — see section 4 for the details:
no `QJsonObject&` overload (4a), per-iteration pointer compilation in indexed
loops (4b), and per-call path revalidation for repeated appends under the same
prefix (4c).

## 2. JSONPath parameter binding

`ConnectorJsonRest` needs "the included entry of type `user` whose `id` equals
a runtime value". The natural expression is
`$[?(@.type=='user' && @.id=='<uuid>')]`, but interpolating runtime strings
into a query language is injection-shaped (a single `'` changes the filter,
and escaping rules are the consumer's problem). The spike adopted the policy
*static query strings only; dynamic matching in code*, which works but splits
one logical query into query + loop (see `ProcessVenueEntriesReply`, and the
`EditorIdWithQuoteCannotInjectIntoTypeFilter` test documenting the policy).

Suggested: named placeholders bound at evaluation time, so compiled queries
stay cacheable:

```cpp
auto path = JSONPath::create(u"$[?(@.type=='user' && @.id==$editorId)]");
auto result = path->evaluate(doc, {{u"editorId", editorUserId}});
```

## 3. C++17 consumer facade

The library exposes `std::expected` in its public headers, so every including
TU must compile as C++23. In ac-develop that was free for `src/` and `tests/`
(already C++23) and cheap for `dbVenueDatabase` (PRIVATE `cxx_std_23` bump,
usage confined to one `.cpp`). But all `db*` libraries pin `PUBLIC cxx_std_17`
via `cmake/TargetDefaults.cmake`, and some (e.g. `dbACWebUpdate`, a
JsonFileSettingsBackend candidate) may not be able to bump freely once usage
reaches public headers.

ac-develop already vendors `libraries/stdcompat` whose `stdcompat/expected`
aliases `std::expected` when available and falls back to `tl::expected` on
C++17. A build option (e.g. `JSON_QUERY_EXPECTED_COMPAT=ON`) that lets the
result alias resolve through such a shim — or a small C++17 facade header —
would let the older libraries adopt the query APIs without a standard bump.
This is a nice-to-have; the PRIVATE-bump pattern proved workable.

## 4. First-consumer API observations (v0.6.0 write support)

Quirks we hit adopting the write API and the monadic idioms; none blocked the
refactor, all have workarounds in the consuming code.

- **(a) No `QJsonObject&`/`QJsonArray&` write overloads.** Consumers that hold
  a `QJsonObject` (the natural Qt type for a document root that is known to be
  an object, e.g. `MilanDeviceEntity::m_jsonObject`) must round-trip every
  write: `QJsonValue root{object}; pointer.set(root, ...); object =
  root.toObject();` (see the `SetAt` helper in `MilanDeviceEntity.cpp`).
  Overloads mutating `QJsonObject&`/`QJsonArray&` in place would remove the
  ceremony (cheap COW-wise, but boilerplate every consumer will write).
- **(b) No index-parameterized pointers.** Loops over array elements build the
  path per iteration via `QString("/adp_information/interfaces/%1/mac_address")
  .arg(i)` and recompile it. A cheap navigation/composition API (e.g.
  `pointer / i / u"mac_address"` on a compiled prefix, or a compiled template
  with index placeholders) would keep the compile-once pattern in indexed
  loops.
- **(c) Repeated writes under one prefix revalidate the full path each call.**
  Appending N stream mappings to `.../dynamic/dynamic_mappings/-` is N
  independent `set()` walks from the root. `JSONPatch` can batch this, but
  building the operations array in QJson is its own boilerplate — a small
  patch-builder (`patch.add(path, value)...apply(doc)`) or a write cursor
  rooted at a prefix would make batching the easy path.
- **(d) `as<T>` lacks 64-bit integer support.** `JsonTarget` covers
  `QJsonArray|QJsonObject|QString|double|int|bool`; ArrayCalc domain ids are
  `qint64` (Qt's native JSON integer width). Drupal entity ids fit `int`
  today, so `ConnectorJsonRest` narrows through `as<int>` and widens back —
  correct but avoidable. Adding `qint64` (and arguably `uint`) to `as` would
  close the gap.
- **(e) Literal pointers still need dereference ceremony.** `create()`
  returning `std::expected` is right for runtime input, but for known-good
  literals every consumer writes an assert-and-unwrap helper
  (`CompiledPointer`/`CompiledPath` in `ConnectorJsonRest.cpp`). A
  compile-time-validated form (UDL `"/a/b"_json_ptr` or a `consteval` factory)
  would eliminate that class of helper.
- **(f) `Error::message_qt()` returns `QStringView`.** Fine for streaming, but
  QTest macros and `QString` contexts need `formatted_message()`; we reached
  for the wrong one first. A note in the README error-handling section would
  spare the next consumer the compile error.
- **Monadic composition works as advertised** — this was the payoff of (d)/(f)
  being minor: `create(path).and_then(evaluate).and_then(as<T>).value_or(def)`
  replaced both bespoke read helpers, and `as<T>` slots directly into
  `and_then` as a continuation. `{.createIntermediates = true}` designated
  initializers read well at call sites.

## 5. Ergonomic and packaging notes (small)

- **MSVC flag**: the build passes `/templateDepth:2000`, which MSVC's `cl`
  does not know (`D9002: ignoring unknown option`) — it is a clang(-cl) flag;
  gate it on the compiler id.
- **Dependency provisioning**: first configure fetches CTRE and
  tl-function_ref via FetchContent (network). The `FIND_PACKAGE_ARGS` fallback
  already prefers preinstalled packages; publishing vcpkg port names /
  documenting the override for offline CI would round this off. For ac-develop
  we can add `ctre`/`tl-function-ref` to the vcpkg manifest when the spike
  graduates.
- **Licensing**: Apache-2.0 WITH LLVM-exception OR MIT — needs an entry in the
  third-party notices inventory before this leaves spike status (not a library
  change).

## What worked well (no change requested)

- Compile-once/evaluate-many pattern (immutable, thread-safe compiled queries)
  maps cleanly onto function-local `static const` query objects.
- The write API's strong guarantee (document untouched on any error) plus
  `[[nodiscard]]` results made the adoption low-risk; the RFC 6901 `-` append
  token and `createIntermediates` covered the Milan exporter's needs exactly.
- `JSONSchema` validation errors (`instanceLocation` + `message`) gave precise
  diagnostics while authoring the Milan template schema; the all-templates
  test suite (`tests/Export/Data/MilanDeviceEntityTest.cpp`) doubles as the
  schema safety net.
- Embedding via `add_subdirectory` was frictionless: options default OFF when
  not top-level, the library sets its own C++ standard at directory scope, and
  `json_query::json_query` is the only visible target.
