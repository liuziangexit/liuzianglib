#pragma once
#ifndef liuzianglib_singleton
#define liuzianglib_singleton
#include <iostream>
#include <atomic>
#include <mutex>
#include <memory>
//Version 2.4.22V22
//20180709

#ifdef __SSE2__
#include <emmintrin.h>
namespace DC {
	namespace singletonSpace {
		inline void spin_loop_pause() noexcept { _mm_pause(); }
	}
}
#elif defined(_MSC_VER) && _MSC_VER >= 1800 && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
namespace DC {
	namespace singletonSpace {
		inline void spin_loop_pause() noexcept { _mm_pause(); }
	}
}
#else
namespace DC {
	namespace singletonSpace {
		inline void spin_loop_pause() noexcept {}
	}
}
#endif

namespace DC {

	template <typename T, typename AllocatorT = std::allocator<T>>
	class singleton final {
	public:
		singleton() {}

		singleton(const singleton&) = delete;

		~singleton() {
			this->clear();
		}

		void* operator new(std::size_t) = delete;

		void operator delete(void*) = delete;

	public:
		template <typename ...Args>
		T* get_instance(Args ...args) {
			if (m_object == nullptr) {
				std::atomic_thread_fence(std::memory_order_acquire);//load-store
				while (m_flag.test_and_set(std::memory_order_relaxed))
					DC::singletonSpace::spin_loop_pause();
				//有疑问：如何保证store-load
				if (m_object == nullptr) {
					T *tmp = nullptr;
					try {
						tmp = new(m_allocator.allocate(1)) T(std::forward<Args>(args)...);
					}
					catch (std::bad_alloc& ex) {
						throw ex;
					}
					catch (...) {
						throw std::exception("constructor throws an exception");
					}
					std::atomic_thread_fence(std::memory_order_release);//store-store
					m_object = tmp;
					std::atomic_thread_fence(std::memory_order_release);//store-store
					m_flag.clear();
				}
			}
			return m_object;
		}

		void clear()noexcept {
			T* tmp = m_object;
			if (tmp == nullptr)
				return;
			std::atomic_thread_fence(std::memory_order_acquire);//load-store
			while (m_flag.test_and_set(std::memory_order_relaxed))
				DC::singletonSpace::spin_loop_pause();
			std::atomic_thread_fence(std::memory_order_release);//store-store
			m_object = nullptr;
			std::atomic_thread_fence(std::memory_order_release);//store-store
			m_flag.clear();
			tmp->~T();
			m_allocator.deallocate(tmp, 1);
		}

	private:
		T * m_object{ nullptr };
		std::atomic_flag m_flag{ ATOMIC_FLAG_INIT };
		AllocatorT m_allocator;
	};

}

#endif
