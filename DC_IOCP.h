#pragma once
#ifndef liuzianglib_IOCP
#define liuzianglib_IOCP
#include <winsock2.h>
#include <MSWSock.h>
#include "liuzianglib.h"
#include "DC_WinSock.h"
#include "DC_ThreadPool.h"
#include "DC_type.h"
#include "boost\lockfree\queue.hpp"
#pragma comment(lib,"ws2_32.lib")
//Version 2.4.21V14
//20170721

//1.删掉不必要的部分
//2.重写所有代码
//3.使用方法不是继承，而是设置回调函数
//4.main函数使用getch阻塞，可以手动clear或者暂停/开始
//5.cleaner删除套接字资源前使用CancelIoEx清除IOCP内部队列中其上所有IO请求
//6.worker线程检测到客户端已经断开连接或者超时了的话会立刻关闭套接字，但是套接字的资源依然要等cleaner来回收

namespace DC {

	namespace Web {

		namespace IOCP {

			namespace IOCPSpace {

				enum OperationType { ACCEPT, SEND, RECV, EXIT };

				template <typename _Ty>
				class QueueAllocator final {//designed for DC::IOCP
				public:
					using value_type = _Ty;
					using pointer = _Ty*;
					using const_pointer = const pointer;
					using size_type = DC::size_t;

				private:
					using size_t = DC::size_t;

				public:
					QueueAllocator(size_t size) :m_list(size) {}

					QueueAllocator(const QueueAllocator&) = delete;

					~QueueAllocator()noexcept {
						this->clear();
					}

				public:
					//create a new value_type object in list and return the pointer of the new object.
					//if malloc or construction failed,it will return false.
					//thread-safe but maybe not lock-free(depending on the implementation of malloc)
					template <typename ...ARGS>
					pointer make(ARGS&& ...args)noexcept {
						auto ptr = reinterpret_cast<pointer>(malloc(sizeof(value_type)));
						if (ptr == NULL)
							return nullptr;

						try {
							new(ptr) value_type(std::forward<ARGS>(args)...);
							if (!m_list.push(ptr)) {
								destory(ptr);
								ptr = nullptr;
							}
						}
						catch (...) {
							free(ptr);
							ptr = nullptr;
						}
						return ptr;
					}

					inline bool put(const value_type& obj)noexcept {
						try {
							return m_list.push(obj);
						}
						catch (...) {
							return false;
						}
					}

					template <typename Functor>
					bool remove_if(const Functor& _Pred)noexcept {//deconstruct and deallocate the back item in the queue, if _Pred return true.
																  //otherwise push the item(already popped) back to queue.
																  //_Pred should not throw exception
																  //if _Pred has been executed, return true, otherwise return false
						pointer ptr = nullptr;
						if (!m_list.pop(ptr))
							return false;

						bool PredRv;
						try {
							PredRv = _Pred(*ptr);
						}
						catch (...) {
							if (!m_list.push(ptr))
								destory(ptr);
							return true;
						}

						try {
							if (!PredRv) {
								if (!m_list.push(ptr))
									destory(ptr);
							}
							else
								destory(ptr);
						}
						catch (...) {}

						return true;
					}

					void clear()noexcept {//not thread-safe		 
						pointer ptr = nullptr;
						while (m_list.pop(ptr)) {
							if (ptr == nullptr) continue;
							destory(ptr);
						}

						m_list.reserve(0);
					}

					bool empty()const {
						return m_list.empty();
					}

				private:
					inline void destory(pointer ptr) {
						ptr->~value_type();
						free(ptr);
					}

				private:
					boost::lockfree::queue<pointer> m_list;
				};

				struct IOContext {};

				class SocketContext {};

				bool AssociateWithIOCP();

				bool PostOperation();

				bool PostRecv();

				bool PostSend();

				bool PostExit();

				class dc_iocp_server final{
				public:
					dc_iocp_server(const DC::size_t threadnumber, const DC::WinSock::Address& listenaddress) :m_iocp(nullptr), m_listenSocket(INVALID_SOCKET), m_threadNumber(threadnumber),
						m_tp(nullptr), m_listenAddress(listenaddress), m_pscPool(0) {
					}

					dc_iocp_server(const DC::size_t threadnumber, const std::string& listenip, const DC::size_t& listenport) :m_iocp(nullptr), m_listenSocket(INVALID_SOCKET), m_threadNumber(threadnumber),
						m_tp(nullptr), m_listenAddress(DC::WinSock::MakeAddr(listenip, listenport)), m_pscPool(0) {
					}

					dc_iocp_server(const dc_iocp_server&) = delete;

					dc_iocp_server& operator=(const dc_iocp_server&) = delete;

				public:
					bool start();

					void stop();

				public:
					void set_client_max_alive_time();

					void set_cleaner_wakeup_time();

					void set_cleaner_max_block_time();

					void set_recv_buffer_size();

					void set_callback_onRecv();

					void set_callback_onSend();

					void set_callback_onExcept();

				private:
					void listener();

					void worker();

					void cleaner();

				private:
					HANDLE m_iocp;
					DC::WinSock::Socket m_listenSocket;
					DC::ThreadPool *m_tp;

					const std::size_t m_threadNumber;
					DC::WinSock::Address m_listenAddress;

					QueueAllocator<SocketContext> m_pscPool;
				};

			}

			using Server = IOCPSpace::dc_iocp_server;

		}

	}

}

#endif