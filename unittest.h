#pragma once

// Custom state-of-the-art unit testing functionality

#if 1
#define UNITTEST static_assert
#else
#include <cassert>
#ifdef __cpp_placeholder_variables
#define xassert(e, sz) static bool _ = (assert((sz, e)), 'U');
#else
#define xassert(e, sz) assert((e));
#endif
#define UNITTEST xassert
#endif
