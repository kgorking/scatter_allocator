#pragma once

// Custom state-of-the-art unit testing functionality

#if 1
#define UNITTEST static_assert
#else
#include <cassert>
#ifdef __cpp_placeholder_variables
#define xassert(e, sz) static char _ = (assert((e) && (sz)), 'U');
#else
#define xassert(e, sz) assert((e) && (sz));
#endif
#define UNITTEST xassert
#endif
