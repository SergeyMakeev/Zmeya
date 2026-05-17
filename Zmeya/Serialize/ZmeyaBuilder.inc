template <typename TRoot> class Builder : public BuilderBase
{
  public:
    explicit Builder(size_t initialSizeInBytes, PrivateToken tk)
        : BuilderBase(initialSizeInBytes, tk)
    {
        // Automatically allocate the root object at offset 0
        allocate<TRoot>();
    }

    using TSelf = Builder<TRoot>;
    static std::unique_ptr<TSelf> create(size_t initialSizeInBytes = kDefaultWriteBlobArenaReserveBytes)
    {
        std::unique_ptr<TSelf> res = std::make_unique<TSelf>(initialSizeInBytes, PrivateToken{});
        return res;
    }

    ZMEYA_NODISCARD ArenaRef<TRoot> getRoot() noexcept { return ArenaRef<TRoot>(this, goffset_t(0)); }
};
