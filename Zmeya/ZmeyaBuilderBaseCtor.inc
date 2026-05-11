    goffset_t arena_byte_offset_of(const void* ptr) const
    {
        ZMEYA_ASSERT(contains_pointer(ptr));
        offset_t allocOffset = offset_t(uintptr_t(ptr) - uintptr_t(data.data()));
        // check that global offset fit
        ZMEYA_ASSERT((allocOffset >= 0 && allocOffset < std::numeric_limits<goffset_t>::max()) && "Offset is too big, more that 2GB?");
        return goffset_t(allocOffset);
    }

    explicit BuilderBase(size_t initialSizeInBytes, PrivateToken)
    {
        // 16 bytes minimum
        size_t reserveSize = std::max(initialSizeInBytes, size_t(16));
        data.reserve(reserveSize);

        // Static assertions for zm types
        static_assert(std::is_trivially_copyable<Pointer<int>>::value, "Pointer is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<Array<int>>::value, "Array is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<HashSet<int>>::value, "HashSet is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<zm::Pair<int, float>>::value, "Pair is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<HashMap<int, int>>::value, "HashMap is_trivially_copyable check failed");
        static_assert(std::is_trivially_copyable<String>::value, "String is_trivially_copyable check failed");
    }

    ~BuilderBase() = default;

    // Non-copyable, non-movable to keep it simple
    BuilderBase(const BuilderBase&) = delete;
    BuilderBase& operator=(const BuilderBase&) = delete;
    BuilderBase(BuilderBase&&) = delete;
    BuilderBase& operator=(BuilderBase&&) = delete;
