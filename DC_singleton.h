#pragma once
#ifndef liuzianglib_singleton
#define liuzianglib_singleton
#include <iostream>
#include <atomic>
#include <mutex>
#include <memory>
//Version 2.4.22V3
//20180116

namespace DC {
    
    template <typename T, typename LockT = std::mutex, typename AllocatorT = std::allocator<T>>
    class singleton final {
    public:
        singleton() {}
        
        singleton(const singleton&) = delete;
        
        void* operator new(std::size_t) = delete;
        
        void operator delete(void*) = delete;
        
    public:
        template <typename ...Args>
        T* get_instance(Args ...args) {
            if(m_object == nullptr) {
                std::atomic_thread_fence(std::memory_order_acquire);
                std::unique_lock<LockT> ulocker(m_lock);
                if(m_object == nullptr) {
                    std::atomic_thread_fence(std::memory_order_release);
                    m_object = new(m_allocator.allocate(1)) T(std::forward<Args>(args)...);
                }
            }
            return m_object;
        }
        
        void clear();
        
    private:
        T *m_object {nullptr};
        LockT m_lock;
        AllocatorT m_allocator;
    };

}

#endif
