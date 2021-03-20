#include "TestHelper.h"

namespace utils
{

std::vector<char> copyBytes(Zmeya::Span<char> from)
{
    std::vector<char> res;
    res.resize(from.size);
    for (size_t i = 0; i < from.size; i++)
    {
        res[i] = from.data[i];
    }
    return res;
}

} // namespace Utils