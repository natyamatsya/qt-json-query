# JSON Schema Draft 2020-12 Compliance Roadmap

**Current Status:** 85% Unit Test Pass Rate (74/87 tests passing)  
**Target:** 100% Compliance with JSON Schema Draft 2020-12  
**Last Updated:** January 4, 2026

---

## Current Test Results

### ✅ Passing Categories (74/87 tests - 85%)

**Core Validation (100%)**
- ✅ Boolean schemas (true/false)
- ✅ Empty schema validation
- ✅ Type constraints (string, number, integer, boolean, null, array, object)

**Numeric Constraints (100%)**
- ✅ minimum / maximum
- ✅ exclusiveMinimum / exclusiveMaximum
- ✅ multipleOf

**String Constraints (100%)**
- ✅ minLength / maxLength
- ✅ pattern (regex validation)

**Array Constraints (100%)**
- ✅ items (Draft 2020-12 style)
- ✅ prefixItems (tuple validation)
- ✅ contains / minContains / maxContains
- ✅ minItems / maxItems
- ✅ uniqueItems

**Object Constraints (Partial - 80%)**
- ✅ properties
- ✅ required
- ✅ additionalProperties
- ✅ minProperties / maxProperties
- ❌ patternProperties (failing)
- ❌ propertyNames (failing)

**Combinators (100%)**
- ✅ allOf
- ✅ anyOf
- ✅ oneOf
- ✅ not

**Conditionals (100%)**
- ✅ if / then / else

**Value Constraints (100%)**
- ✅ enum
- ✅ const

**Format Validators (93%)**
- ✅ date-time, date
- ❌ time (regex issue)
- ✅ email, idn-email
- ✅ hostname, idn-hostname
- ✅ ipv4, ipv6
- ✅ uri, uri-reference, iri, iri-reference, uri-template
- ✅ uuid
- ✅ json-pointer, relative-json-pointer
- ✅ regex

**References (Partial - 33%)**
- ❌ $ref to $defs (failing)
- ❌ $ref to root (failing)
- ❌ $ref nested (failing)
- ❌ $anchor (failing)
- ✅ Recursive schemas (basic)
- ❌ $ref in combinators (failing)
- ✅ $ref in conditionals

**Dependencies (0%)**
- ❌ dependentRequired (failing)
- ❌ dependentSchemas (failing)

**Unevaluated (Not Tested)**
- ⏳ unevaluatedProperties (implemented, needs testing)
- ⏳ unevaluatedItems (implemented, needs testing)

**Dynamic References (Not Implemented)**
- ⏳ $dynamicRef (not implemented)
- ⏳ $dynamicAnchor (not implemented)

---

## Milestone 1: Fix $ref Resolution (CRITICAL) 🔴

**Priority:** Highest  
**Impact:** 8 failing tests  
**Estimated Effort:** 2-3 days  
**Status:** Not Started

### Problem
$ref resolution is not working in the compilation phase. References to $defs, root, and anchors are not being resolved correctly.

### Failing Tests
1. `JSONSchemaRefTest.RefToDefinitions`
2. `JSONSchemaRefTest.RefToRoot`
3. `JSONSchemaRefTest.RefToNestedDefinition`
4. `JSONSchemaRefTest.MultipleRefsToSameDefinition`
5. `JSONSchemaRefTest.AnchorReference`
6. `JSONSchemaRefTest.MutuallyRecursiveSchemas`
7. `JSONSchemaRefTest.RefInCombinators`
8. `JSONSchemaRefTest.NestedRefs`

### Implementation Tasks
- [ ] Implement $ref fragment parsing in `JSONSchemaCompile.cpp`
- [ ] Add $defs collection during schema compilation
- [ ] Implement JSON Pointer resolution for $ref targets
- [ ] Add $anchor registration and lookup
- [ ] Handle recursive $ref resolution
- [ ] Add circular reference detection
- [ ] Update `RefSchema` node to store resolved target index
- [ ] Test $ref in all contexts (properties, combinators, conditionals)

### Files to Modify
- `src/json-query/json-schema/JSONSchemaCompile.cpp`
- `include/json-query/json-schema/internal/SchemaNode.hpp`

### Acceptance Criteria
- ✅ All 8 $ref tests pass
- ✅ $ref to $defs works
- ✅ $ref to root (#) works
- ✅ $anchor resolution works
- ✅ Recursive schemas work
- ✅ Circular references detected and handled

---

## Milestone 2: Fix Pattern Properties & Property Names 🟡

**Priority:** High  
**Impact:** 2 failing tests  
**Estimated Effort:** 4-6 hours  
**Status:** Not Started

### Problem
Pattern properties and property names validation not working correctly.

### Failing Tests
1. `JSONSchemaKeywordTest.PatternProperties`
2. `JSONSchemaKeywordTest.PropertyNames`

### Implementation Tasks
- [ ] Debug pattern properties validation in `JSONSchemaValidate.cpp`
- [ ] Verify QRegularExpression matching for property names
- [ ] Ensure pattern properties are evaluated correctly
- [ ] Test propertyNames schema application

### Files to Modify
- `src/json-query/json-schema/JSONSchemaValidate.cpp`

### Acceptance Criteria
- ✅ Pattern properties validation works
- ✅ Property names schema validation works
- ✅ Both tests pass

---

## Milestone 3: Fix Dependent Required/Schemas 🟡

**Priority:** High  
**Impact:** 2 failing tests  
**Estimated Effort:** 4-6 hours  
**Status:** Not Started

### Problem
Dependent required and dependent schemas not validating correctly.

### Failing Tests
1. `JSONSchemaKeywordTest.DependentRequired`
2. `JSONSchemaKeywordTest.DependentSchemas`

### Implementation Tasks
- [ ] Debug dependentRequired logic in `JSONSchemaValidate.cpp`
- [ ] Verify dependentSchemas compilation and validation
- [ ] Ensure conditional schema application works
- [ ] Test with multiple dependencies

### Files to Modify
- `src/json-query/json-schema/JSONSchemaValidate.cpp`
- Possibly `src/json-query/json-schema/JSONSchemaCompile.cpp`

### Acceptance Criteria
- ✅ dependentRequired validation works
- ✅ dependentSchemas validation works
- ✅ Both tests pass

---

## Milestone 4: Fix Time Format Validator 🟢

**Priority:** Medium  
**Impact:** 1 failing test  
**Estimated Effort:** 1-2 hours  
**Status:** Not Started

### Problem
Time format regex pattern not matching valid time strings correctly.

### Failing Tests
1. `JSONSchemaFormatTest.TimeFormat`

### Implementation Tasks
- [ ] Review RFC 3339 time format specification
- [ ] Update time format regex in `FormatValidators.cpp`
- [ ] Test with various time formats (with/without timezone, milliseconds)

### Files to Modify
- `src/json-query/json-schema/internal/FormatValidators.cpp`

### Acceptance Criteria
- ✅ Time format validation works for all RFC 3339 time formats
- ✅ Test passes

---

## Milestone 5: Official Test Suite Integration 🔵

**Priority:** High  
**Impact:** Comprehensive compliance validation  
**Estimated Effort:** 1 day  
**Status:** Not Started

### Tasks
- [ ] Add JSON Schema Test Suite as git submodule
  ```bash
  git submodule add https://github.com/json-schema-org/JSON-Schema-Test-Suite.git compliance/json-schema-test-suite
  git submodule update --init --recursive
  ```
- [ ] Build compliance test runner
- [ ] Run Draft 2020-12 compliance tests
- [ ] Analyze failures and categorize by feature
- [ ] Create issue tracker for compliance failures

### Acceptance Criteria
- ✅ Test suite submodule added
- ✅ Compliance tests build successfully
- ✅ Baseline compliance metrics established
- ✅ All mandatory tests pass (target: 95%+)

---

## Milestone 6: Dynamic References Implementation 🔵

**Priority:** Medium  
**Impact:** Advanced schema composition  
**Estimated Effort:** 2-3 days  
**Status:** Not Started

### Problem
$dynamicRef and $dynamicAnchor not implemented. These enable advanced schema composition patterns.

### Implementation Tasks
- [ ] Implement $dynamicAnchor registration during compilation
- [ ] Add dynamic scope tracking during validation
- [ ] Implement $dynamicRef resolution with scope chain
- [ ] Handle dynamic anchor inheritance
- [ ] Test with complex schema composition scenarios

### Files to Modify
- `src/json-query/json-schema/JSONSchemaCompile.cpp`
- `src/json-query/json-schema/JSONSchemaValidate.cpp`
- `include/json-query/json-schema/internal/SchemaNode.hpp`

### Acceptance Criteria
- ✅ $dynamicAnchor registration works
- ✅ $dynamicRef resolution works
- ✅ Dynamic scope chain maintained correctly
- ✅ Official test suite dynamic reference tests pass

---

## Milestone 7: Full Draft 2020-12 Compliance 🎯

**Priority:** Highest (Final Goal)  
**Impact:** Production-ready implementation  
**Estimated Effort:** 1-2 weeks (after Milestones 1-6)  
**Status:** Not Started

### Requirements
- ✅ 100% of unit tests passing (87/87)
- ✅ 95%+ of official test suite passing
- ✅ All mandatory keywords implemented
- ✅ All format validators working
- ✅ Performance benchmarks established
- ✅ Documentation complete

### Tasks
- [ ] Fix all remaining test failures
- [ ] Optimize performance bottlenecks
- [ ] Add missing edge case handling
- [ ] Complete API documentation
- [ ] Write usage examples
- [ ] Create integration guide
- [ ] Performance profiling and optimization
- [ ] Memory usage optimization

### Acceptance Criteria
- ✅ All unit tests pass (87/87)
- ✅ Official test suite: 95%+ pass rate
- ✅ No known bugs in implemented features
- ✅ Performance meets or exceeds targets
- ✅ Documentation complete and accurate
- ✅ Ready for production use

---

## Timeline Estimate

### Phase 1: Critical Fixes (Week 1)
- **Days 1-3:** Milestone 1 - Fix $ref resolution
- **Day 4:** Milestone 2 - Fix pattern properties & property names
- **Day 5:** Milestone 3 - Fix dependent required/schemas
- **Day 5:** Milestone 4 - Fix time format validator

**Target:** 100% unit test pass rate (87/87 tests)

### Phase 2: Compliance Testing (Week 2)
- **Days 1-2:** Milestone 5 - Official test suite integration
- **Days 3-5:** Fix compliance test failures
- **Day 5:** Establish baseline compliance metrics

**Target:** 90%+ official test suite pass rate

### Phase 3: Advanced Features (Week 3)
- **Days 1-3:** Milestone 6 - Dynamic references implementation
- **Days 4-5:** Additional compliance test fixes

**Target:** 95%+ official test suite pass rate

### Phase 4: Polish & Documentation (Week 4)
- **Days 1-2:** Performance optimization
- **Days 3-4:** Documentation
- **Day 5:** Final testing and validation

**Target:** Milestone 7 - Full compliance achieved

---

## Success Metrics

| Metric | Current | Milestone 1 | Milestone 5 | Final Goal |
|--------|---------|-------------|-------------|------------|
| **Unit Tests** | 74/87 (85%) | 87/87 (100%) | 87/87 (100%) | 87/87 (100%) |
| **Official Suite** | Not run | Not run | 90%+ | 95%+ |
| **$ref Support** | 33% | 100% | 100% | 100% |
| **Format Validators** | 93% | 100% | 100% | 100% |
| **Dependencies** | 0% | 100% | 100% | 100% |
| **Dynamic Refs** | 0% | 0% | 0% | 100% |

---

## Risk Assessment

### High Risk
- **$ref Resolution Complexity** - Most critical feature, affects many tests
  - *Mitigation:* Start with simple cases, add complexity incrementally
  - *Fallback:* Focus on most common $ref patterns first

### Medium Risk
- **Official Test Suite Unknowns** - Don't know exact failure count
  - *Mitigation:* Run early, categorize failures, prioritize by impact
  - *Fallback:* Focus on mandatory features first

### Low Risk
- **Format Validators** - Isolated, well-defined
- **Pattern Properties** - Likely simple bug fix
- **Dependencies** - Implementation exists, likely validation bug

---

## Dependencies & Blockers

### External Dependencies
- JSON Schema Test Suite (git submodule)
- Qt 6.7+ (already available)
- MSVC 2022 or Clang (already configured)

### Internal Dependencies
- Milestone 1 blocks: Milestones 2, 3 (need working $ref for complex tests)
- Milestone 5 blocks: Milestone 7 (need compliance metrics)
- Milestone 6 independent: Can be done in parallel with 1-5

### No Known Blockers
- Build environment: ✅ Working
- Test infrastructure: ✅ Working
- Core implementation: ✅ Complete

---

## Next Actions (Priority Order)

1. **Start Milestone 1** - Fix $ref resolution (CRITICAL)
   - Begin with simple $ref to $defs
   - Add $anchor support
   - Handle recursive references
   - Test thoroughly

2. **Quick Wins** - Milestones 2, 3, 4 in parallel
   - Fix time format (1-2 hours)
   - Fix pattern properties (2-3 hours)
   - Fix dependencies (2-3 hours)

3. **Compliance Testing** - Milestone 5
   - Add test suite submodule
   - Run and analyze results
   - Create fix priority list

4. **Advanced Features** - Milestone 6
   - Implement dynamic references
   - Test with official suite

5. **Final Push** - Milestone 7
   - Fix remaining failures
   - Optimize and document
   - Release v1.0

---

## Conclusion

The JSON Schema implementation is **85% complete** with a clear path to 100% compliance. The main blocker is $ref resolution, which is critical but well-scoped. With focused effort on the milestones above, full Draft 2020-12 compliance is achievable within 3-4 weeks.

The implementation quality is high, following all project standards, and the test infrastructure is solid. Once $ref resolution is fixed, the remaining issues are relatively minor and can be addressed quickly.

**Estimated Time to Full Compliance:** 3-4 weeks  
**Confidence Level:** High  
**Recommendation:** Proceed with Milestone 1 immediately
