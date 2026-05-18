#include "ZmeyaConfig.h"

#include <cstdio>
#include <cstdlib>

namespace zm
{

void onAssertionFailed(const char* expression, const char* srcFile, unsigned int srcLine)
{
    std::fprintf(stderr, "Zmeya assertion failed: %s (%s:%u)\n", expression, srcFile, static_cast<unsigned>(srcLine));
    std::abort();
}

} // namespace zm
