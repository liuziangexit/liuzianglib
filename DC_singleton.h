#pragma once
#ifndef liuzianglib_singleton
#define liuzianglib_singleton
#include <atomic>
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

	namespace singletonSpace {

		class FlagHolder {
		public:
			FlagHolder(std::atomic_flag& input) :m_flag(input) {}

			~FlagHolder() {
				this->m_flag.clear(std::memory_order_release);
			}

		private:
			std::atomic_flag& m_flag;
		};

	}

	template <typename T, typename AllocatorT = std::allocator<T>>
	class singleton final {
	public:
		singleton() {}

		singleton(const singleton&) = delete;

		~singleton()noexcept {
			this->clear();
		}

		void* operator new(std::size_t) = delete;

		void operator delete(void*) = delete;

	public:
		//try to create an instance and return it
		//if an instance already exists, return it
		template <typename ...Args>
		T* try_create_instance(Args ...args) {
			lock_flag();
			DC::singletonSpace::FlagHolder fh(this->m_flag);

			if (this->m_instance == nullptr) {
				try {
					this->m_instance = new(m_allocator.allocate(1)) T(std::forward<Args>(args)...);
				}
				catch (std::bad_alloc& ex) {
					throw ex;
				}
				catch (...) {
					throw std::exception("constructor throws an exception");
				}
			}

			return this->m_instance;
		}

		//read only
		//return nullptr if there is no instance yet
		T* get_instance()noexcept {
			lock_flag();
			DC::singletonSpace::FlagHolder fh(this->m_flag);

			return this->m_instance;
		}

		//try to destory instance
		//if there is no instance, do nothing
		void clear()noexcept {
			lock_flag();
			DC::singletonSpace::FlagHolder fh(this->m_flag);

			if (this->m_instance != nullptr) {
				this->m_instance->~T();
				this->m_allocator.deallocate(this->m_instance, 1);
				this->m_instance = nullptr;
			}
		}

	private:
		void lock_flag()noexcept {
			while (this->m_flag.test_and_set(std::memory_order_acquire))
				DC::singletonSpace::spin_loop_pause();
		}

	private:
		T * m_instance{ nullptr };
		std::atomic_flag m_flag{ ATOMIC_FLAG_INIT };
		AllocatorT m_allocator;
	};

}

#endif
