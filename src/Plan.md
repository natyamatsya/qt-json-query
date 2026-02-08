include/json‑query/
├── JSONPath.hpp           // public façade + inline friend decls
├── ContainerCursor.hpp    // your tagged‑pointer cursor
├── JSONQueryUtils.hpp     // small string/regex helpers
└── QtHash.hpp             // hash support

src/json‑path/
├── JSONPathCreate.cpp     // JSONPath::create(…), detectTrailingFunction
├── JSONPathCompile.cpp    // JSONPath::compilePath(…)
//   └─ parseDot(), parseBracket(), parseBare(), makeSlice(), stripOuterParens(), splitTopLevel
├── JSONPathFilter.cpp     // JSONPath::compileFilter(…)
//   └─ parseOr/And/In/Compare/Regex rule fns + callCompileFilter wrapper
├── JSONPathEvaluate.cpp   // JSONPath::evaluate(…), evaluateAll(…), evaluateTokenStream()
//   └─ dispatch table & detail::eval<Key/Index/Slice/...>
├── JSONPathEvalHelpers.cpp// detail::fanOut(), addsMultiplicity(), squash(), applyTrailing()
└── JSONPathRecursion.cpp  // wildcardObject/Array, evaluateRecursive(), evalSlice(), normalizeIndex()
