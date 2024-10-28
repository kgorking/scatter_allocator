#pragma once

// Custom state-of-the-art unit testing functionality

#if 1
#define UNITTEST static_assert
#else
#include <cassert>
#define xassert(e, sz) assert((e))
#define UNITTEST xassert
#endif
