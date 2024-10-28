module;
#include <cassert>
#include <span>
#include <memory>
#include <vector>
#include <bit>
export module kg.scatter_allocator;

namespace kg {
	template <typename Fn, typename T>
	concept callback_takes_a_span = std::invocable<Fn, std::span<T>>;

	template<size_t N>
	concept power_of_two = (std::popcount(N) == 1);

	// 'Scatter Allocator'
	// 
	// * A single allocation can result in many addresses being returned, as the
	//   allocator fills in holes in the internal pools of memory.
	// * No object construction/destruction happens.
	// * It is not thread safe.
	// * Deallocated memory is reused before new memory is taken from pools.
	//   This way old pools will be filled with new data before newer pools are tapped.
	//   Filling it 'from the back' like this should keep fragmentation down.
	export
	template <typename T, std::size_t InitialSize = 16, std::size_t MaximumPoolSize = 8 * 1024 * 1024/* *1024 */>
		requires (power_of_two<InitialSize> && power_of_two<MaximumPoolSize>)
	struct scatter_allocator {
		static_assert(InitialSize > 0);

		constexpr scatter_allocator() noexcept = default;
		constexpr scatter_allocator(scatter_allocator const&) noexcept = delete;
		constexpr scatter_allocator(scatter_allocator&& other) noexcept = delete;
		constexpr scatter_allocator& operator=(scatter_allocator const&) = delete;
		constexpr scatter_allocator& operator=(scatter_allocator&& other) = delete;

		[[nodiscard]]
		constexpr std::span<T> allocate_one() {
			std::span<T> t;
			allocate_with_callback(1, [&t](std::span<T> s) {
				assert(t.empty());
				assert(s.size() == 1);
				t = s;
				});
			return t;
		}

		// EXTENT !!
		constexpr void allocate_with_callback(std::size_t const count, callback_takes_a_span<T> auto&& alloc_callback) {
			std::size_t remaining_count = count;

			// Take space from free list
			while (!free_list.empty()) {
				auto& free_span = free_list.back();

				std::size_t const min_space = std::min(remaining_count, free_span.size());
				std::span<T> span = free_span.subspan(0, min_space);
				alloc_callback(span);

				if (min_space == free_span.size()) {
					// All the free space was used, so remove the span from the free list
					free_list.pop_back();
				}
				else {
					// Update the free span size and return
					free_span = free_span.subspan(min_space + 1);
					return;
				}

				remaining_count -= min_space;
				if (remaining_count == 0)
					return;
			}

			// Take space from pools
			pools.alloc(remaining_count, alloc_callback); // TODO forward
		}

		constexpr void deallocate(std::span<T> const span) {
#ifdef _DEBUG
			// Poison the allocation
			if (!std::is_constant_evaluated())
				std::memset(span.data(), 0xee, span.size_bytes());
#endif

			// Add it to the free list
			free_list.push_back(span);
		}

	private:
		constexpr auto* add_pool() {
			return pools.add_next();
		}

		template<std::size_t N>
		struct pool {
			std::size_t next_available = 0;
			std::span<T, N> data{ std::allocator<T>{}.allocate(N), N };
			std::unique_ptr<pool<2 * N>> next;

			constexpr ~pool() {
				std::allocator<T>{}.deallocate(data.data(), N);
			}

			constexpr void add_next() {
				if (!next)
					next = std::make_unique<pool<2 * N>>();
			}

			constexpr void alloc(std::size_t remaining_count, auto&& callback) {
				if (next_available == N) {
					add_next();
					next->alloc(remaining_count, callback);
					return;
				}

				std::size_t const min_space = std::min({ remaining_count, (data.size() - next_available) });
				assert(min_space > 0);

				auto span = data.subspan(next_available, min_space);// std::span<T>{ data.data() + next_available, min_space };
				callback(span);

				next_available += min_space;
				remaining_count -= min_space;

				if (remaining_count > 0) {
					add_next();
					next->alloc(remaining_count, callback);
				}
			}
		};

		pool<InitialSize> pools;
		std::vector<std::span<T>> free_list;
	};
} // namespace kg
