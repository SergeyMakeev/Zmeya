# Zmeya [![Build Status](https://travis-ci.org/SergeyMakeev/Zmeya.svg?branch=main)](https://travis-ci.org/SergeyMakeev/Zmeya) [![Build status](https://ci.appveyor.com/api/projects/status/qllqgshfy9cjme2q?svg=true)](https://ci.appveyor.com/project/SergeyMakeev/zmeya/) [![codecov](https://codecov.io/gh/SergeyMakeev/Zmeya/branch/main/graph/badge.svg?token=V07QJQX2NT)](https://codecov.io/gh/SergeyMakeev/Zmeya) ![MIT](https://img.shields.io/badge/license-MIT-blue.svg)

Zmeya is a header-only C++11 binary serialization library designed for games and performance-critical applications.
Zmeya is not even a serialization library in the usual sense but rather a set of STL-like containers that entirely agnostic for their memory location and movable. As long as you use Zmeya data structures + other trivially_copyable, everything just works, and there is no deserialization cost. You can memory-map a serialized file and immediately start using your data. There are no pointers fixup, nor any other parsing/decoding needed. You can also serialize your data and send it over the network, there is no deserialization cost on the receiver side, or you can even use Zmeya for interprocess communication.

# Features

- Cross-platform compatible
- Single header library (~550 lines of code for deserialization and extra 750 lines of code with serialization support enabled)
- No code generation required: no IDL or metadata, just use your types directly
- No macros
- Heavily optimized for performance
- No dependencies
- Zmeya pointers are always 32-bits (configurable) regardless of the target platform pointer size

Zmeya library offering the following memory movable types
- `Pointer<T>`
- `Array<T>`
- `String`
- `HashSet<Key>`
- `HashMap<Key, Value>`

# Usage

Include the header file, and you are all set.

# Usage example

Here is a simple usage example.

```cpp
#include "Zmeya.h"

struct Test
{
  uint32_t someVar;
  zm::String name;
  zm::Pointer<Test> ptr;
  zm::Array<zm::String> arr;
  zm::HashMap<zm::String, float> hashMap;
};

int main()
{
   // load binary file to memory (using fread, mmap, etc)
   // no parsing/decoding needed
   const Test* test = (const Test*)loadBytesFromDisk("binaryFile.zm");  
   
   // use your loaded data
   printf("%s\n", test->name.c_str());
   for(const zm::String& str : test->arr)
   {
     printf("%s\n",str.c_str());
   }
   printf("key = %3.2f\n", test->hashMap.find("key", 0.0f));
   return 0;
}
```

You can always find more usage examples looking into unit test files. They are organized in a way to covers all Zmeya features and shows common usage patterns.

# How it works

Zmeya movable containers' key idea is to use self-relative pointers instead of using “absolute” pointers provided by C++ by default.
The idea is pretty simple; instead of using the absolute address, we are using offset relative to the pointer's memory address.
i.e., `target_address = uintptr_t(this) + offset`

One of the problems of such offset-based addressing is the representation of the null pointer. The null pointer can't be safely represented like an offset since the absolute address 0 is always outside of the mapped region.
So we decided to use offset 1 as a special magic value that encodes null pointer. Using offset 1 as the magic value for the null pointer means that the pointer can't point to the byte after its own pointer, which is usually not a problem.
Also, for convenience, we decided to bias everything by -1, which makes the null pointer encoded by zero, which is great because it fits very well with zero initialization.
So the final absolute addres resolve logic looks like this
`target_address = uintptr_t(this) + offset + 1`

Which is perfectly representable by one of x86 addressing modes
`addr = base_reg + index_reg + const_8`

Here is an example of generated assembly code
https://godbolt.org/z/v6sj6s

```cpp
#include <stdint.h>

template<typename T>
struct OffsetPtr {
    int32_t offset;
    T* get() const noexcept {
        return reinterpret_cast<T*>(uintptr_t(this) + 1 + offset);
    }
};

template<typename T>
struct Ptr {
    T* ptr;
    T* get() const noexcept {
        return ptr;
    }
};

int test1(const OffsetPtr<int>& ptr) {
    return *ptr.get();
}

int test2(const Ptr<int>& ptr) {
    return *ptr.get();
}
```

```asm
test1(OffsetPtr<int> const&):
        movsx   rax, DWORD PTR [rdi]
        mov     eax, DWORD PTR [rax+1+rdi]
        ret
        
test2(Ptr<int> const&):
        mov     rax, QWORD PTR [rdi]
        mov     eax, DWORD PTR [rax]
        ret
```

So there is no extra overhead from using such pointers in comparison with traditional pointers.

All other Zmeya containers are pretty much based on the same principles.
i.e.
`zm::String` is a self-relative pointer to `const char*`
`zm::Array<T>` is a self-relative pointer to data + size
`zm::HashSet<Key>` is made using two arrays (buckets and values)
etc...

The only requirement is that we have to have all the data tightly packed in a single memory region or binary blob.
Zmeya provides a convenient mechanism to build such a binary blob called `zm::BlobBuilder`
Blob builder is capable of convert all the standard STL containers to appropriate zmeya movable containers. Blob builder also provides a mechanism to convert all the inner types (e.g., std::vector<std::string>) to Zmeya compatible type. And by default, Zmeya offers convertors/template specializations for all commonly used cases.

# References

Boost::offset_ptr<T>   
https://www.boost.org/doc/libs/1_75_0/doc/html/interprocess/offset_ptr.html

Handmade Hero forum thread  
https://hero.handmade.network/forums/code-discussion/t/487-serialization_techniques_with_memory_pooling


Relative Pointers article by Ginger Bill  
https://www.gingerbill.org/article/2020/05/17/relative-pointers/

"The Blob and I" by Niklas Gray  
https://bitsquid.blogspot.com/2010/02/blob-and-i.html

FlatBuffers by Google  
https://google.github.io/flatbuffers/

Physics Optimization Strategies by Sergiy Migdalskiy (slides 56-68)  
http://media.steampowered.com/apps/valve/2015/Migdalskiy_Sergiy_Physics_Optimization_Strategies.pdf

https://youtu.be/Nsf2_Au6KxU?t=1542


Relative Pointers by Jonathan Blow  
https://www.youtube.com/watch?v=Z0tsNFZLxSU





