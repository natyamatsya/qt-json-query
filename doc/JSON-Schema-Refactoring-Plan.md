# JSON Schema Validation Refactoring Plan

**Goal:** Refactor JSON Schema validation code to match the high-quality modular architecture of the JSONPath implementation.

---

## Current Architecture Issues

### Current File Structure
```
src/json-query/json-schema/
├── JSONSchema.cpp (2,178 bytes)
├── JSONSchemaCompile.cpp (24,145 bytes) - TOO LARGE
├── JSONSchemaValidate.cpp (24,304 bytes) - TOO LARGE
└── internal/
    ├── FormatValidators.cpp
    └── FormatValidators.hpp
```

### Problems Identified

1. **Monolithic files** - Validation and compilation logic in single large files
2. **No helper modules** - All logic inline, no separation of concerns
3. **Missing performance optimizations** - No arena allocators, no object pools
4. **Poor modularity** - Hard to test individual validation functions
5. **Inline validation logic** - Makes code hard to read and maintain
6. **❌ If-else cascades** - Current code uses nested if-else instead of dispatch tables
7. **❌ No dispatch mechanism** - Unlike JSONPath's TableGen-inspired dispatch system
8. **❌ Type switching** - Uses std::variant visitation instead of optimized dispatch

---

## Target Architecture (Based on JSONPath)

### Proposed File Structure
```
src/json-query/json-schema/
├── JSONSchema.cpp
├── JSONSchemaCompile.cpp (main compilation orchestration)
├── JSONSchemaValidate.cpp (main validation orchestration with dispatch)
├── JSONSchemaValidateHelpers.cpp (validation utilities)
├── JSONSchemaValidateType.cpp (type validation)
├── JSONSchemaValidateNumeric.cpp (numeric constraints)
├── JSONSchemaValidateString.cpp (string constraints)
├── JSONSchemaValidateArray.cpp (array constraints)
├── JSONSchemaValidateObject.cpp (object constraints)
├── JSONSchemaValidateCombinators.cpp (allOf, anyOf, oneOf, not)
└── internal/
    ├── FormatValidators.cpp
    ├── FormatValidators.hpp
    ├── ValidationContext.hpp (validation context structure)
    ├── ValidationHelpers.hpp (inline helper functions)
    ├── ValidationDispatchTable.hpp (TableGen-inspired dispatch)
    └── SchemaNodeDispatch.hpp (SchemaNode type dispatch)
```

---

## Refactoring Steps

### Phase 1: Extract Helper Functions (Low Risk)

**Goal:** Move utility functions to dedicated helper files

**Files to Create:**
1. `JSONSchemaValidateHelpers.cpp/hpp`
   - `jsonValuesEqual()` - Compare JSON values
   - `jsonValueToSchemaType()` - Type conversion
   - Path manipulation utilities

**Benefits:**
- Easier to test
- Reusable across validation modules
- Cleaner main validation file

### Phase 2: Modularize Validation by Type (Medium Risk)

**Goal:** Split validation logic into focused modules

**Files to Create:**

1. **JSONSchemaValidateType.cpp**
   ```cpp
   void validateType(ValidateContext& ctx, 
                     const TypeConstraint& constraint,
                     const QJsonValue& instance,
                     const QString& instancePath,
                     const QString& schemaPath);
   ```

2. **JSONSchemaValidateNumeric.cpp**
   ```cpp
   void validateNumeric(ValidateContext& ctx,
                        const ObjectSchema& node,
                        double value,
                        const QString& instancePath,
                        const QString& schemaPath);
   ```

3. **JSONSchemaValidateString.cpp**
   ```cpp
   void validateString(ValidateContext& ctx,
                       const ObjectSchema& node,
                       QStringView str,
                       const QString& instancePath,
                       const QString& schemaPath);
   ```

4. **JSONSchemaValidateArray.cpp**
   ```cpp
   void validateArray(ValidateContext& ctx,
                      const ObjectSchema& node,
                      const QJsonArray& arr,
                      const QString& instancePath,
                      const QString& schemaPath);
   ```

5. **JSONSchemaValidateObject.cpp**
   ```cpp
   void validateObject(ValidateContext& ctx,
                       const ObjectSchema& node,
                       const QJsonObject& obj,
                       const QString& instancePath,
                       const QString& schemaPath);
   ```

6. **JSONSchemaValidateCombinators.cpp**
   ```cpp
   void validateCombinators(ValidateContext& ctx,
                            const ObjectSchema& node,
                            const QJsonValue& instance,
                            const QString& instancePath,
                            const QString& schemaPath);
   ```

**Benefits:**
- Each module is focused and testable
- Easier to optimize individual validators
- Better code organization
- Follows JSONPath pattern

### Phase 3: Dispatch Mechanism Implementation (Critical - JSONPath Pattern)

**Goal:** Replace if-else cascades with TableGen-inspired dispatch tables

**Current Problem:**
```cpp
// Current: std::variant visitation with if-else cascade
std::visit([&](const auto& schemaVariant) {
    using T = std::decay_t<decltype(schemaVariant)>;
    if constexpr (std::is_same_v<T, BooleanSchema>) {
        // validate boolean schema
    } else if constexpr (std::is_same_v<T, ObjectSchema>) {
        // validate object schema
    } else if constexpr (std::is_same_v<T, RefSchema>) {
        // validate ref schema
    }
}, node);
```

**Solution: Dispatch Table System**

1. **Create ValidationDispatchTable.hpp**
   ```cpp
   namespace json_query::json_schema::internal {
   
   // Schema node type metadata
   struct SchemaNodeMetadata {
       const char* name;
       bool requiresRecursion;
       bool canShortCircuit;
       int priority;
   };
   
   // Dispatch function type
   using SchemaValidatorFn = void (*)(ValidateContext&, 
                                      const SchemaNode&,
                                      const QJsonValue&,
                                      const QString&,
                                      const QString&);
   
   // Forward declare validator functions
   void validateBooleanSchema(ValidateContext&, const SchemaNode&, 
                             const QJsonValue&, const QString&, const QString&);
   void validateObjectSchema(ValidateContext&, const SchemaNode&,
                            const QJsonValue&, const QString&, const QString&);
   void validateRefSchema(ValidateContext&, const SchemaNode&,
                         const QJsonValue&, const QString&, const QString&);
   
   // Compile-time dispatch table (O(1) lookup)
   constexpr std::array<SchemaValidatorFn, 3> SCHEMA_DISPATCH_TABLE = {
       validateBooleanSchema,  // index 0
       validateObjectSchema,   // index 1
       validateRefSchema       // index 2
   };
   
   constexpr std::array<SchemaNodeMetadata, 3> SCHEMA_METADATA_TABLE = {
       SchemaNodeMetadata{"boolean", false, true, 100},
       SchemaNodeMetadata{"object", true, false, 50},
       SchemaNodeMetadata{"ref", true, false, 90}
   };
   
   // Dispatcher class
   class SchemaNodeDispatcher {
   public:
       static void dispatch(ValidateContext& ctx,
                          const SchemaNode& node,
                          const QJsonValue& instance,
                          const QString& instancePath,
                          const QString& schemaPath) {
           const auto typeIndex = node.index();
           SCHEMA_DISPATCH_TABLE[typeIndex](ctx, node, instance, 
                                           instancePath, schemaPath);
       }
   };
   
   } // namespace
   ```

2. **Create Constraint Dispatch System**
   ```cpp
   // For ObjectSchema validation, dispatch by constraint type
   enum class ConstraintType {
       Type,
       Numeric,
       String,
       Array,
       Object,
       Combinator
   };
   
   // Constraint validator function type
   using ConstraintValidatorFn = void (*)(ValidateContext&,
                                         const ObjectSchema&,
                                         const QJsonValue&,
                                         const QString&,
                                         const QString&);
   
   // Dispatch table for constraint validation
   constexpr std::array<ConstraintValidatorFn, 6> CONSTRAINT_DISPATCH_TABLE = {
       validateTypeConstraint,
       validateNumericConstraints,
       validateStringConstraints,
       validateArrayConstraints,
       validateObjectConstraints,
       validateCombinatorConstraints
   };
   ```

3. **Type-Based Dispatch for JSON Values**
   ```cpp
   // Dispatch based on QJsonValue type
   enum class JsonValueType : uint8_t {
       Null = 0,
       Bool = 1,
       Number = 2,
       String = 3,
       Array = 4,
       Object = 5
   };
   
   inline constexpr JsonValueType getJsonValueType(const QJsonValue& v) noexcept {
       return static_cast<JsonValueType>(v.type());
   }
   
   // Fast type-specific validation dispatch
   using TypeValidatorFn = void (*)(ValidateContext&,
                                    const ObjectSchema&,
                                    const QJsonValue&,
                                    const QString&,
                                    const QString&);
   
   constexpr std::array<TypeValidatorFn, 6> TYPE_DISPATCH_TABLE = {
       validateNullValue,
       validateBoolValue,
       validateNumberValue,
       validateStringValue,
       validateArrayValue,
       validateObjectValue
   };
   ```

**Benefits:**
- ✅ O(1) dispatch instead of if-else cascade
- ✅ Compile-time optimization opportunities
- ✅ Extensible without modifying core logic
- ✅ Matches JSONPath architecture
- ✅ Better branch prediction
- ✅ Easier to add new schema types

### Phase 4: Performance Optimizations (High Impact)

**Goal:** Add performance optimizations similar to JSONPath

**Optimizations to Add:**

1. **Inline Helper Functions**
   - Move hot-path helpers to `internal/ValidationHelpers.hpp`
   - Mark with `inline` or `[[gnu::always_inline]]`
   - Example: `shouldContinue()`, type checks

2. **Reduce Allocations**
   - Use `QStringView` instead of `QString` where possible
   - Avoid unnecessary `QString` copies in error messages
   - Pre-allocate error message buffers

3. **Optimize Type Checking**
   ```cpp
   // Optimized: Direct check with dispatch
   inline bool isTypeValid(const QJsonValue& v, const TypeConstraint& c) noexcept {
       const auto jsonType = getJsonValueType(v);
       return c.allowsJsonType(jsonType);  // O(1) lookup
   }
   ```

4. **Cache Regex Compilation**
   - Already done for pattern properties
   - Ensure all regexes are pre-compiled and optimized

5. **Validation Context Optimization**
   ```cpp
   struct ValidateContext {
       const CompiledSchema& schema;
       ValidationResult& result;
       bool stopOnFirstError;
       
       // Inline fast path check
       [[nodiscard]] [[gnu::always_inline]] 
       inline bool shouldContinue() const noexcept {
           return !stopOnFirstError || result.isValid();
       }
   };
   ```

### Phase 5: Internal Namespace Organization

**Goal:** Move performance-critical structures to `internal/`

**Files to Create:**

1. **internal/ValidationContext.hpp**
   ```cpp
   namespace json_query::json_schema::internal {
       struct ValidateContext {
           const CompiledSchema& schema;
           ValidationResult& result;
           bool stopOnFirstError;
           
           [[nodiscard]] [[gnu::always_inline]]
           inline bool shouldContinue() const noexcept {
               return !stopOnFirstError || result.isValid();
           }
       };
   }
   ```

2. **internal/ValidationHelpers.hpp**
   ```cpp
   namespace json_query::json_schema::internal {
       // Inline helper functions for hot paths
       [[gnu::always_inline]] inline bool isInteger(double d) noexcept;
       [[gnu::always_inline]] inline bool matchesType(const QJsonValue& v, SchemaType t) noexcept;
       
       // Fast JSON value type checking
       [[gnu::always_inline]] 
       inline constexpr JsonValueType getJsonValueType(const QJsonValue& v) noexcept {
           return static_cast<JsonValueType>(v.type());
       }
   }
   ```

---

## Implementation Strategy

### Step 1: Extract Helpers (Safe, No Behavior Change)
1. Create `JSONSchemaValidateHelpers.cpp/hpp`
2. Move `jsonValuesEqual()` and utilities
3. Update includes in `JSONSchemaValidate.cpp`
4. **Test:** All tests should still pass

### Step 2: Extract Type Validation (Focused Module)
1. Create `JSONSchemaValidateType.cpp/hpp`
2. Move `validateType()` function
3. Update includes
4. **Test:** All tests should still pass

### Step 3: Extract Numeric Validation
1. Create `JSONSchemaValidateNumeric.cpp/hpp`
2. Move `validateNumeric()` function
3. **Test:** All tests should still pass

### Step 4: Extract String Validation
1. Create `JSONSchemaValidateString.cpp/hpp`
2. Move `validateString()` function
3. **Test:** All tests should still pass

### Step 5: Extract Array Validation
1. Create `JSONSchemaValidateArray.cpp/hpp`
2. Move `validateArray()` function
3. **Test:** All tests should still pass

### Step 6: Extract Object Validation
1. Create `JSONSchemaValidateObject.cpp/hpp`
2. Move `validateObject()` function
3. **Test:** All tests should still pass

### Step 7: Extract Combinators
1. Create `JSONSchemaValidateCombinators.cpp/hpp`
2. Move combinator validation
3. **Test:** All tests should still pass

### Step 8: Implement Dispatch Tables (Critical)
1. Create `internal/ValidationDispatchTable.hpp`
2. Define dispatch function types and tables
3. Implement SchemaNodeDispatcher
4. Replace std::visit with dispatch calls
5. **Test:** All tests pass, verify O(1) dispatch

### Step 9: Performance Optimizations
1. Create `internal/ValidationHelpers.hpp`
2. Add inline functions
3. Optimize hot paths with dispatch
4. **Test:** All tests pass, measure performance

### Step 10: Update CMakeLists.txt
Add all new source files to the build system.

---

## Expected Benefits

### Code Quality
- ✅ Modular, focused files (< 500 lines each)
- ✅ Better separation of concerns
- ✅ Easier to test individual validators
- ✅ Matches JSONPath architecture quality
- ✅ **No if-else cascades** - TableGen-inspired dispatch
- ✅ **O(1) dispatch** - Compile-time dispatch tables

### Performance
- ✅ Inline hot-path functions
- ✅ Reduced allocations
- ✅ Better cache locality
- ✅ Optimized type checking
- ✅ **Fast dispatch** - Array lookup instead of branching
- ✅ **Better branch prediction** - Dispatch table pattern
- ✅ **Compile-time optimization** - constexpr dispatch tables

### Maintainability
- ✅ Easy to find validation logic
- ✅ Clear file organization
- ✅ Easier to add new validators
- ✅ Better code review experience
- ✅ **Extensible dispatch** - Add validators without core changes
- ✅ **Declarative style** - Metadata tables like JSONPath

---

## Risk Assessment

### Low Risk
- Extracting helper functions
- Moving code to new files without changes
- Adding inline functions

### Medium Risk
- Changing function signatures
- Optimizing hot paths
- Refactoring validation context

### Mitigation
- **Test after each step**
- Keep changes small and focused
- Maintain 100% test pass rate
- Measure performance before/after

---

## Success Criteria

1. ✅ All 87 tests still passing (100%)
2. ✅ No performance regression (ideally improvement)
3. ✅ All files < 1000 lines
4. ✅ Clear module boundaries
5. ✅ Matches JSONPath code quality
6. ✅ Easy to add new validation keywords

---

## Timeline Estimate

- **Phase 1 (Helpers):** 1-2 hours
- **Phase 2 (Modularization):** 3-4 hours
- **Phase 3 (Performance):** 2-3 hours
- **Phase 4 (Internal namespace):** 1-2 hours

**Total:** 7-11 hours of focused work

---

## Next Steps

1. Get approval for refactoring plan
2. Start with Phase 1 (low risk)
3. Test after each module extraction
4. Measure performance improvements
5. Document any API changes
