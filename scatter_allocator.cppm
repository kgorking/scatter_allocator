module;
#include <cassert>
#include <span>
#include <memory>
#include <array>
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
			std::unique_ptr<free_block>* ptr_free = &free_list;
			while (*ptr_free) {
				free_block* ptr = ptr_free->get();
				std::size_t const min_space = std::min(remaining_count, ptr->span.size());
				if (min_space == 0) {
					ptr_free = &ptr->next;
					continue;
				}

				std::span<T> span = ptr->span.subspan(0, min_space);
				alloc_callback(span);

				remaining_count -= min_space;
				if (remaining_count == 0)
					return;

				if (min_space == ptr->span.size()) {
					auto next = std::move(ptr->next);
					*ptr_free = std::move(next);
				}
				else {
					ptr->span = ptr->span.subspan(min_space + 1);
					ptr_free = &ptr->next;
				}
			}

			// Take space from pools
			pools.alloc(remaining_count, alloc_callback); // TODO forward
		}

		constexpr void deallocate(std::span<T> const span) {
			assert(validate_addr(span) && "Invalid address passed to deallocate()");

#ifdef _DEBUG
			// Poison the allocation
			if (!std::is_constant_evaluated())
				std::memset(span.data(), 0xee, span.size_bytes());
#endif

			// Add it to the free list
			free_list = std::make_unique<free_block>(std::move(free_list), span);
		}

	private:
		constexpr auto* add_pool() {
			return pools.add_next();
		}

		static constexpr bool valid_addr(T const* p, T const* begin, T const* end) {
			// It is undefined behavior to compare pointers directly,
			// so use distances instead. This also works at compile time.
			return true;
			//auto const size = std::distance(begin, end);
			//return std::distance(begin, p) >= 0 && std::distance(p, end) <= size;
		}

		constexpr bool validate_addr(std::span<T> const span) {
			return pools.valid_span(span);
		}

		template<std::size_t N>
		struct pool {
			std::size_t next_available = 0;
			alignas(T) std::array<T, N> data;
			std::unique_ptr<pool<2 * N>> next;

			constexpr bool valid_span(std::span<T> span) const {
				if (valid_addr(span.data(), &data.front(), &data.back()))
					return true;

				if (next)
					return next->valid_span(span);

				return false;
			}

			constexpr void add_next() {
				if (!next)
					next = std::make_unique<pool<2 * N>>(0);
			}

			constexpr void alloc(std::size_t remaining_count, auto&& callback) {
				if (next_available == N) {
					add_next();
					next->alloc(remaining_count, callback);
					return;
				}

				std::size_t const min_space = std::min({ remaining_count, (data.size() - next_available) });
				assert(min_space > 0);

				auto span = std::span<T>{ data.data() + next_available, min_space };
				callback(span);

				next_available += min_space;
				remaining_count -= min_space;

				if (remaining_count > 0) {
					add_next();
					next->alloc(remaining_count, callback);
				}
			}
		};

		template<>
		struct pool<MaximumPoolSize> {
			std::size_t next_available = 0;
			alignas(T) std::array<T, MaximumPoolSize> data;

			constexpr bool valid_span(std::span<T> span) const {
				return valid_addr(span.data(), &data.front(), &data.back());
			}

			constexpr void alloc(std::size_t remaining_count, auto&& callback) {
				assert(next_available < MaximumPoolSize);

				std::size_t const min_space = std::min({ remaining_count, (data.size() - next_available) });
				assert(min_space > 0);

				auto span = std::span<T>{ data.data() + next_available, min_space };
				callback(span);

				next_available += min_space;
				remaining_count -= min_space;

				assert(remaining_count == 0);
			}
		};

		struct free_block {
			std::unique_ptr<free_block> next;
			std::span<T> span;
		};

		pool<InitialSize> pools;
		std::unique_ptr<free_block> free_list;
	};
} // namespace kg
