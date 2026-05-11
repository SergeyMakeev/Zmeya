#pragma once

#include "ZmeyaArray.h"
#include "ZmeyaHash.h"

namespace zm
{

/*

**Hash table key adapters**

Open addressing in `HashSet` / `HashMap` is implemented indirectly via bucket ranges; these adapters
normalize hashing and equality for native keys, `std::string`, and C-string probes into `String` keys.

*/

template <typename Item> struct HashKeyAdapterGeneric
{
    typedef Item ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hasher(item); }
    static bool eq(const ItemType& a, const ItemType& b) { return a == b; }
};

struct HashKeyAdapterStdString
{
    typedef std::string ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hashString(item.c_str()); }
    static bool eq(const ItemType& a, const ItemType& b) { return a == b; }
};

template <typename Item> struct HashKeyValueAdapterGeneric
{
    typedef Item ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hasher(item.first); }
    static bool eq(const ItemType& a, const ItemType& b) { return a.first == b.first; }
};

template <typename Value> struct HashKeyValueAdapterStdString
{
    typedef std::pair<const std::string, Value> ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hashString(item.first.c_str()); }
    static bool eq(const ItemType& a, const ItemType& b) { return a.first == b.first; }
};

struct HashKeyAdapterCStr
{
    typedef const char* ItemType;
    static size_t hash(const ItemType& item) { return HashUtils::hashString(item); }
    static bool eq(const String& a, const ItemType& b) { return a == b; }
};

} // namespace zm
