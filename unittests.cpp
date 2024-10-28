import kg.scatter_allocator;
#include <span>
#include <cassert>
#include <memory>
#include "unittest.h"

using namespace kg;

static bool unittests = []() {
	UNITTEST([] {
		constexpr std::size_t elems_to_alloc = 123;
		scatter_allocator<int> alloc;
		std::size_t total_alloc = 0;
		alloc.allocate_with_callback(elems_to_alloc, [&](std::span<int> s) {
			total_alloc += s.size();
			});
		return elems_to_alloc == total_alloc;
		}(), "allocates correctly");

	UNITTEST([] {
		scatter_allocator<int> alloc;
		std::span<int> subspan;
		alloc.allocate_with_callback(10, [&subspan](auto r) {
			assert(subspan.empty());
			subspan = r.subspan(3, 4);
			});
		alloc.deallocate(subspan);
		return true;
		}(), "frees correctly");

	UNITTEST(([] {
		scatter_allocator<int, 16> alloc;
		std::span<int> span;
		alloc.allocate_with_callback(10, [&span](auto r) {
			assert(span.empty());
			span = r;
			});
		alloc.deallocate(span.subspan(2, 2));
		alloc.deallocate(span.subspan(4, 2));

		// Fills in the two holes (2+2), the rest of the first pool (6),
		// and remaining in new second pool (10)
		int count = 0;
		std::size_t sizes[] = { 2, 2, 6, 10 };
		alloc.allocate_with_callback(20, [&](auto span) {
			assert(sizes[count] == span.size() && "unexpected span size");
			count += 1;
			});
		return (count == 4);
		}()), "scatters correctly");

	UNITTEST([] {
		constexpr std::size_t elems_to_alloc = 16;
		scatter_allocator<int> alloc;
		std::span<int> span;
		alloc.allocate_with_callback(elems_to_alloc, [&](std::span<int> s) {
			span = s;
			});
		for (int& i : span) {
			std::construct_at(&i);
			std::destroy_at(&i);
		}
		alloc.deallocate(span);
		return true;
		}(), "works with construction/destruction");

	return true;
	}();
