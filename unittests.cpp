import kg.scatter_allocator;
#include <span>
#include <string>
#include <cassert>
#include <memory>
#include <vector>
#include "unittest.h"

using namespace kg;

int main() {
	UNITTEST([] {
		constexpr std::size_t elems_to_alloc = 123;
		scatter_allocator<int> alloc;
		std::size_t total_alloc = 0;
		std::vector<std::span<int>> spans;
		alloc.allocate_with_callback(elems_to_alloc, [&](std::span<int> s) {
			total_alloc += s.size();
			spans.push_back(s);
			});

		for (auto span : spans)
			alloc.deallocate(span);

		return elems_to_alloc == total_alloc;
		}(), "allocates correctly");

	UNITTEST([] {
		scatter_allocator<int> alloc;
		std::span<int> span;
		alloc.allocate_with_callback(10, [&span](auto r) {
			assert(span.empty());
			span = r;
			});
		alloc.deallocate(span);

		return true;
		}(), "frees correctly");

	UNITTEST([] {
		scatter_allocator<int> alloc;
		std::span<int> span;
		alloc.allocate_with_callback(10, [&span](auto r) {
			assert(span.empty());
			span = r;
			});
		alloc.deallocate(span);

		int count = 0;
		alloc.allocate_with_callback(11, [&count](auto r) {
			count += r.size();
			});
		assert(count == 11);

		return true;
		}(), "alloc/free/allocs correctly");

	UNITTEST(([] {
		scatter_allocator<std::string, 16> alloc;
		std::vector<std::span<std::string>> spans;
		alloc.allocate_with_callback(10, [&spans](auto r) {
			assert(spans.empty());
			spans.push_back(r);
			});
		alloc.deallocate(spans[0].subspan(2, 2));
		alloc.deallocate(spans[0].subspan(4, 2));

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

	UNITTEST([] {
		scatter_allocator<int> alloc;
		for(int i=0; i<17; i++)
			std::span<int> span = alloc.allocate_one();
		return true;
		}(), "many small allocations");

	puts("All good.");
	return 0;
}
