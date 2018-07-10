#pragma once
#ifndef liuzianglib_singleton
#define liuzianglib_singleton
#include <iostream>
#include <atomic>
#include <mutex>
#include <memory>
//Version 2.4.22V22
//20180709

namespace DC {

	template <typename T, typename LockT = std::mutex, typename AllocatorT = std::allocator<T>>
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
				std::atomic_thread_fence(std::memory_order_acquire);
				std::unique_lock<LockT> ulocker(m_lock);
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
					std::atomic_thread_fence(std::memory_order_release);
					m_object = tmp;
				}
			}
			return m_object;
		}

		void clear()noexcept {
			T* tmp = m_object;
			std::atomic_thread_fence(std::memory_order_acquire);
			if (tmp == nullptr)
				return;
			std::atomic_thread_fence(std::memory_order_release);
			m_object = nullptr;
			std::unique_lock<LockT> ulocker(m_lock);
			tmp->~T();
			m_allocator.deallocate(tmp, 1);
		}

	private:
		T * m_object{ nullptr };
		LockT m_lock;
		AllocatorT m_allocator;
	};

}

#endif
