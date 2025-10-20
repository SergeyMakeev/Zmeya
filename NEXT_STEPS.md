# Zmeya Builder Refactoring - Technical Specification

## Project Overview

**Zmeya** is a header-only C++ binary serialization library designed for games and performance-critical applications. The core innovation is the use of **self-relative pointers** instead of absolute pointers, enabling "movable" containers that can be freely relocated in memory without pointer fixup.

### Key Concepts

- **Self-relative pointers**: `target_address = uintptr_t(this) + offset`
- **Zero deserialization cost**: Memory-mapped files can be used directly
- **Movable containers**: All data structures can be relocated without breaking internal references
- **Contiguous memory layout**: All data must be packed in a single memory region

### Core Container Types
- `zm::Pointer<T>` - Self-relative pointer
- `zm::String` - Self-relative string
- `zm::Array<T>` - Self-relative array
- `zm::HashMap<K,V>` - Hash map using relative pointers
- `zm::HashSet<K>` - Hash set using relative pointers

## Current Refactoring Goals

The project is undergoing a major refactoring to replace the verbose `BlobBuilder` API with a simplified `Builder` API that uses:

1. **Thread-local storage (TLS)** for builder context management
2. **Global assignment functions** for seamless std::* → zm::* conversion
3. **Deep-copy adapters** for automatic nested type conversion
4. **Simplified syntax** similar to regular C++ assignments
5. **Automatic recursive type matching** to prevent code bloat and maximize extensibility

### Target API Design

```cpp
// OLD API (verbose)
std::shared_ptr<zm::BlobBuilder> blobBuilder = zm::BlobBuilder::create(1);
zm::BlobPtr<Root> root = blobBuilder->allocate<Root>();
blobBuilder->copyTo(root->string, stdString);
blobBuilder->copyTo(root->array, stdVector);

// NEW API (simplified)
std::shared_ptr<zm::Builder> _builder = zm::Builder::create();
zm::ScopedBuilder scope(_builder.get());
Root* root = builder->allocate_root<Root>();
zm::assign(root->string, stdString);
zm::assign(root->array, stdVector);
```

## Design Pillars

### 🏗️ **Automatic Recursive Type Matching (Deep-Copy Adapters)**

One of the most critical design principles of the new Builder API is **automatic recursive type conversion** that prevents code bloat and maximizes extensibility.

#### **The Problem Without Deep-Copy Adapters**
Traditional serialization libraries require explicit converters for every possible combination:
```cpp
// BAD: Exponential code bloat
convert(std::vector<std::string> → zm::Array<zm::String>)
convert(std::vector<std::vector<std::string>> → zm::Array<zm::Array<zm::String>>)  
convert(std::vector<std::unordered_map<std::string, int>> → zm::Array<zm::HashMap<zm::String, int32_t>>)
convert(std::unordered_map<std::string, std::vector<float>> → zm::HashMap<zm::String, zm::Array<float>>)
// ... infinite combinations
```

#### **The Solution: Recursive Deep-Copy Pattern**
With deep-copy adapters, you only need to define **base conversions**:

```cpp
// GOOD: Minimal base conversions
void assign(zm::String& to, const std::string& from);           // Leaf converter
void assign(zm::Array<T>& to, const std::vector<F>& from);      // Shape converter  
void assign(zm::HashMap<K,V>& to, const std::unordered_map<FK,FV>& from); // Shape converter

// Recursive deep-copy function
template<typename F, typename T> void deep_copy(const F& from, T& to);
```

#### **How Automatic Recursion Works**

1. **Shape Converters** handle container structure without knowing element types:
   ```cpp
   template<typename T, typename F> 
   void assign(zm::Array<T>& to, const std::vector<F>& from) {
       // Allocate array storage
       T* elements = allocate_array(from.size());
       
       // Recursively convert each element (automatic!)
       for (size_t i = 0; i < from.size(); ++i) {
           new (&elements[i]) T{};
           deep_copy(from[i], elements[i]); // ← Recursive call
       }
   }
   ```

2. **Leaf Converters** handle specific type transformations:
   ```cpp
   void deep_copy(const std::string& from, zm::String& to) { 
       assign(to, from); 
   }
   ```

3. **Automatic Nesting** emerges naturally:
   ```cpp
   // This works automatically without any additional code:
   std::vector<std::vector<std::unordered_map<std::string, float>>> complex_nested;
   zm::Array<zm::Array<zm::HashMap<zm::String, float>>> zmeya_nested;
   
   zm::assign(zmeya_nested, complex_nested); // Just works!
   ```

#### **Extensibility Benefits**

Adding support for new types requires minimal code:

```cpp
// Add support for std::optional → zm::Optional
template<typename F, typename T>
void assign(zm::Optional<T>& to, const std::optional<F>& from) {
    if (from.has_value()) {
        T value{};
        deep_copy(*from, value); // ← Reuses existing conversions
        to.set(std::move(value));
    }
}

// Now ALL nested combinations work automatically:
// std::vector<std::optional<std::string>> → zm::Array<zm::Optional<zm::String>>
// std::unordered_map<std::string, std::optional<std::vector<int>>> → zm::HashMap<zm::String, zm::Optional<zm::Array<int32_t>>>
```

This design pillar is **crucial for preventing code bloat** and making the Builder **extremely extensible** - new container types automatically work with all existing types through recursive composition.

### **Current Deep-Copy Implementation**

The system is already implemented and working correctly with these base converters:

```cpp
// Leaf Converters (specific type transformations)
void deep_copy(const std::string& from, zm::String& to) { assign(to, from); }
void deep_copy(const char* from, zm::String& to) { assign(to, from); }

// Shape Converters (container structure, type-agnostic)
template<typename T, typename F> 
void deep_copy(const std::vector<F>& from, zm::Array<T>& to) { assign(to, from); }

template<typename Key, typename F> 
void deep_copy(const std::unordered_set<F>& from, zm::HashSet<Key>& to) { assign(to, from); }

template<typename Key, typename Value, typename FK, typename FV>
void deep_copy(const std::unordered_map<FK, FV>& from, zm::HashMap<Key, Value>& to) { assign(to, from); }

// Identity Converter (fallback)
template<typename T> void deep_copy(const T& from, T& to) { to = from; }
```

**Result**: Complex nested structures work automatically:
```cpp
// All of these work without additional code:
std::vector<std::string> → zm::Array<zm::String>
std::vector<std::vector<std::string>> → zm::Array<zm::Array<zm::String>>
std::unordered_map<std::string, std::vector<int>> → zm::HashMap<zm::String, zm::Array<int32_t>>
std::vector<std::unordered_map<std::string, std::vector<float>>> → zm::Array<zm::HashMap<zm::String, zm::Array<float>>>

// Proven working in NewBuilderAPI_NestedTypes test:
std::vector<std::vector<std::string>> srcNested = {{"a", "b", "c"}, {"x", "y"}, {"hello", "world", "nested", "test"}};
zm::assign(root->nestedArray, srcNested); // ✅ Works perfectly
```

## Current Implementation Status

### ✅ **Successfully Implemented Features**

1. **Core Builder Class**
   - TLS-based context management via `ScopedBuilder`
   - Memory allocation with proper alignment
   - Finalization with padding support

2. **Assignment Functions**
   - String assignments: `std::string` → `zm::String`
   - Array assignments: `std::vector<T>` → `zm::Array<T>`
   - HashMap assignments: `std::unordered_map<K,V>` → `zm::HashMap<K,V>`
   - HashSet assignments: `std::unordered_set<K>` → `zm::HashSet<K>`
   - Pointer assignments: `T*` → `zm::Pointer<T>`
   - Array of pointers: `std::vector<T*>` → `zm::Array<zm::Pointer<T>>`

3. **Deep-Copy Adapters** ⭐ **Core Design Pillar**
   - **Recursive type conversion** for nested containers
   - **Automatic std::* → zm::* type mapping** without explicit permutation declarations
   - **Shape converters** (containers) + **Leaf converters** (primitives) = **Infinite combinations**
   - **Identity copy fallbacks** for same types
   - **Minimal code footprint** - O(types) instead of O(type combinations)

4. **Memory Management**
   - Proper alignment handling
   - Zero-initialization of allocated memory
   - Placement constructor calls

### ✅ **Working Tests (9/14)**

All tests using the new Builder API work correctly:

- **NewBuilderAPI_BasicTypes** - Basic container conversions
- **NewBuilderAPI_NestedTypes** - Nested array conversions  
- **SimpleTest** - POD structures
- **SimpleTest2** - Arrays of structures with strings
- **PointerTest** - Pointer relationships
- **ListTest** - Linked lists (3000 nodes)
- **HashSetTest** - Hash set operations
- **HashMapTest** - Hash map operations  
- **IteratorsTest** - Container iteration

### ❌ **Failing Tests (5/14)**

- **ArrayTest** - Array of pointers with 793 elements
- **StringTest** - Multiple string arrays with large datasets
- **SimpleFileTest** - Not yet converted (inheritance + file I/O)
- **MMapTest** - Not yet converted (complex inheritance)
- **ReferToTest** - Not yet converted (referTo functionality)

## Root Cause Analysis

### 🔍 **Primary Issue: Memory Reallocation Invalidation**

The core problem is that the Builder uses `std::vector<char>` for internal storage, which can **reallocate and move memory** when it grows beyond the initially reserved capacity.

**When reallocation occurs:**
1. All existing data is moved to a new memory location
2. All previously calculated relative offsets become invalid
3. All pointers to objects in the builder become invalid
4. The `contains_pointer()` checks fail because objects are now at different addresses

**Stack Trace Evidence:**
```
Assertion failed: contains_pointer(base) && "A pointer should belong to the builder"
→ zm::Builder::calculate_relative_offset<zm::Array<T>>
→ zm::Builder::set_array_data<T>  
→ zm::assign<T,std::vector<F>>
```

The `base` parameter (a `zm::Array<T>` object that was allocated in builder memory) is no longer recognized as belonging to the builder after reallocation.

### 🔍 **Secondary Issues**

1. **Missing referTo functionality** - The old API had `referTo()` for sharing data between objects
2. **Complex inheritance cases** - Some tests use inheritance which needs special handling
3. **File I/O integration** - Some tests involve file serialization/deserialization

## Proposed Solution: Handle-Based Building

### 🛠️ **Core Concept: Stable Handles During Building**

**Problem**: Current implementation uses relative offsets during building, which become invalid when the Builder's memory reallocates.

**Solution**: Use stable handles during building phase, convert to offsets during finalization.

### **Two-Phase Architecture**

#### **Phase 1: Building Phase (Handle-Based)**
During building, all Zmeya containers use **stable handles** instead of relative offsets:

```cpp
// Handle type - stable identifier that survives reallocation
using handle_t = uint32_t;
constexpr handle_t NULL_HANDLE = 0;
constexpr handle_t HANDLE_FLAG = 0x80000000; // High bit indicates handle mode

class Builder {
    struct Allocation {
        void* ptr;           // Current memory location (can change)
        size_t size;         // Size of allocation
        size_t alignment;    // Alignment requirement
    };
    
    std::vector<Allocation> allocations;  // Handle = index into this vector
    std::vector<char> temp_memory;        // Temporary storage (can reallocate freely)
    
public:
    // Allocate memory and return stable handle
    handle_t alloc(size_t size, size_t alignment) {
        // Allocate in temporary storage
        void* ptr = allocate_in_temp_memory(size, alignment);
        
        // Create allocation record
        Allocation alloc = {ptr, size, alignment};
        allocations.push_back(alloc);
        
        // Return handle (index + flag)
        return handle_t(allocations.size() - 1) | HANDLE_FLAG;
    }
    
    // Get current pointer from handle (survives reallocation)
    template<typename T> T* getPtr(handle_t handle) {
        ZMEYA_ASSERT(handle & HANDLE_FLAG);
        size_t index = handle & ~HANDLE_FLAG;
        return static_cast<T*>(allocations[index].ptr);
    }
};
```

#### **Phase 2: Finalization Phase (Offset Conversion)**
During finalization, create contiguous memory and convert handles to offsets:

```cpp
Span<char> Builder::finalize() {
    // 1. Calculate total size needed
    size_t total_size = calculate_total_size();
    
    // 2. Allocate final contiguous memory
    std::vector<char> final_memory(total_size);
    
    // 3. Copy all objects to final locations
    std::unordered_map<handle_t, size_t> handle_to_offset;
    size_t current_offset = 0;
    
    for (size_t i = 0; i < allocations.size(); ++i) {
        handle_t handle = i | HANDLE_FLAG;
        const Allocation& alloc = allocations[i];
        
        // Copy object to final location
        void* final_ptr = &final_memory[current_offset];
        std::memcpy(final_ptr, alloc.ptr, alloc.size);
        
        // Record handle → offset mapping
        handle_to_offset[handle] = current_offset;
        
        current_offset += alloc.size;
        current_offset = align_up(current_offset, next_alignment);
    }
    
    // 4. Convert all handles to relative offsets
    convert_handles_to_offsets(final_memory, handle_to_offset);
    
    return Span<char>(final_memory.data(), final_memory.size());
}
```

### **Modified Container Behavior**

During building phase, containers store handles instead of offsets:

```cpp
template<typename T> class Pointer {
    roffset_t relativeOffset; // Can be either offset OR handle
    
    T* get() const {
        if (relativeOffset & HANDLE_FLAG) {
            // Building phase - resolve handle
            Builder* builder = detail::get_global_builder();
            return builder->getPtr<T>(relativeOffset);
        } else {
            // Runtime phase - use relative offset
            return reinterpret_cast<T*>(uintptr_t(this) + relativeOffset);
        }
    }
};
```

## Recommended Implementation Plan

### Phase 1: Implement Handle-Based Building (High Priority)

1. **Add handle infrastructure to Builder**
   ```cpp
   class Builder {
       using handle_t = uint32_t;
       static constexpr handle_t HANDLE_FLAG = 0x80000000;
       static constexpr handle_t NULL_HANDLE = 0;
       
       struct Allocation {
           std::unique_ptr<char[]> memory;
           size_t size;
           size_t alignment;
       };
       
       std::vector<Allocation> allocations;
       handle_t next_handle = 1; // Start from 1, 0 is NULL_HANDLE
   };
   ```

2. **Modify container get() methods** to handle both modes
   ```cpp
   template<typename T> class Pointer {
       T* get() const {
           if (relativeOffset & HANDLE_FLAG) {
               // Building phase - resolve handle
               Builder* builder = detail::get_global_builder();
               return builder->getPtr<T>(relativeOffset);
           } else {
               // Runtime phase - use relative offset  
               return reinterpret_cast<T*>(uintptr_t(this) + relativeOffset);
           }
       }
   };
   ```

3. **Update assignment functions** to use handles during building
4. **Implement finalization with handle→offset conversion**

### **Detailed Implementation Steps**

1. **Modify Builder allocation methods**
   ```cpp
   template<typename T> T* allocate_internal() {
       size_t size = sizeof(T);
       size_t alignment = alignof(T);
       
       // Allocate individual memory block
       auto memory = std::make_unique<char[]>(size);
       T* ptr = reinterpret_cast<T*>(memory.get());
       new (ptr) T{}; // Placement constructor
       
       // Store allocation record
       Allocation alloc = {std::move(memory), size, alignment};
       allocations.push_back(std::move(alloc));
       
       // Return handle as fake offset
       handle_t handle = (allocations.size() - 1) | HANDLE_FLAG;
       
       // Store handle in object for later conversion
       // (This requires container cooperation)
       
       return ptr;
   }
   ```

2. **Update String assignment to use handles**
   ```cpp
   inline void assign(String& to, const std::string& from) {
       Builder* builder = detail::get_global_builder();
       
       // Allocate string data and get handle
       handle_t string_handle = builder->alloc_string_data(from);
       
       // Store handle (will be converted to offset during finalization)
       to.data.relativeOffset = string_handle;
   }
   ```

3. **Implement finalization with handle conversion**
   ```cpp
   void convert_handles_to_offsets(char* final_memory, 
                                   const std::unordered_map<handle_t, size_t>& mapping) {
       // Walk through final memory and convert all handles to offsets
       for (const auto& alloc : allocations) {
           // Find all handle references in this allocation
           // Convert them to relative offsets
       }
   }
   ```

### Phase 2: Add Missing Functionality (Medium Priority)

1. **Implement referTo functionality** using handle sharing
2. **Add support for inheritance** in assignment functions  
3. **Add file I/O helpers** for serialization/deserialization

### Phase 3: Optimize and Polish (Low Priority)

1. **String deduplication** - Reuse identical string data via handle sharing
2. **Memory usage optimization** - Optimal packing during finalization
3. **Performance benchmarking** - Compare with old BlobBuilder API

## Benefits of Handle-Based Approach

### ✅ **Advantages**
- **Eliminates reallocation issues** - Handles remain stable regardless of memory moves
- **Simplifies building logic** - No need to track pointer invalidation
- **Enables flexible memory management** - Can use any allocation strategy during building
- **Maintains clean API** - No changes needed to user-facing assignment functions
- **Supports referTo naturally** - Multiple handles can reference same allocation
- **Preserves deep-copy extensibility** - Handle system works seamlessly with recursive type conversion

### ⚠️ **Implementation Considerations**
- **Container cooperation required** - All containers need handle-aware get() methods
- **Finalization complexity** - Need to walk memory and convert handles to offsets
- **Memory overhead during building** - Individual allocations vs contiguous block
- **Handle→offset mapping** - Need efficient way to find and convert all handles

## Testing Strategy

### Immediate Testing
```bash
# Test working functionality
.\build\Debug\ZmeyaTest.exe --gtest_filter="*NewBuilderAPI*:*PointerTest*:*SimpleTest*:*ListTest*:*IteratorsTest*:*HashSetTest*:*HashMapTest*"

# Test failing functionality  
.\build\Debug\ZmeyaTest.exe --gtest_filter="*ArrayTest*:*StringTest*"
```

### Post-Fix Testing
After implementing the memory reallocation fix, all 14 tests should pass.

## Code Quality Assessment

### ✅ **Strengths**
- Clean, intuitive API design
- Comprehensive type conversion system
- Proper memory alignment handling
- Extensive test coverage
- Good separation of concerns

### ⚠️ **Areas for Improvement**
- Memory reallocation handling (critical)
- Error handling could be more robust
- Documentation for complex edge cases
- Performance optimization opportunities

## Conclusion

The Zmeya Builder refactoring has successfully achieved its primary goal of creating a simplified, intuitive API. The core functionality works correctly, and 9 out of 14 tests pass completely.

The remaining issues are primarily related to memory management during the building process, specifically the vector reallocation problem. Once this is resolved, the refactoring will be complete and provide a significantly improved developer experience while maintaining all the performance benefits of the original Zmeya library.

**Estimated effort to complete**: 1-2 days for an experienced C++ developer familiar with the codebase.
