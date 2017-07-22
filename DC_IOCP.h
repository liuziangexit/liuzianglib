#pragma once
#ifndef liuzianglib_IOCP
#define liuzianglib_IOCP
#include <stdlib.h>
#include <winsock2.h>
#include <MSWSock.h>
#include "liuzianglib.h"
#include "DC_WinSock.h"
#include "DC_ThreadPool.h"
#include "DC_type.h"
#include "DC_timer.h"
#include "boost\lockfree\queue.hpp"
#pragma comment(lib,"ws2_32.lib")
//Version 2.4.21V17
//20170723

//1.删掉不必要的部分
//2.重写所有代码
//3.使用方法不是继承，而是设置回调函数
//4.main函数使用getch阻塞，可以手动clear或者暂停/开始
//5.cleaner删除套接字资源前使用CancelIoEx清除IOCP内部队列中其上所有IO请求
//6.worker线程检测到客户端已经断开连接或者超时了的话会立刻关闭套接字，但是套接字的资源依然要等cleaner来回收

#define TEMPBUFFERSIZE 1024
void dosth()noexcept {};

namespace DC {

	namespace Web {

		namespace IOCP {

			namespace IOCPSpace {

				class SpinLock final {
				public:
					SpinLock() :flag{ 0 } {}

					SpinLock(const SpinLock&) = delete;

				public:
					void lock() {
						while (flag.test_and_set(std::memory_order::memory_order_acquire));
					}

					bool try_lock() {
						return !flag.test_and_set(std::memory_order_acquire);
					}

					void unlock() {
						flag.clear(std::memory_order::memory_order_release);
					}

				private:
					std::atomic_flag flag;
				};

				enum OperationType { ACCEPT, SEND, RECV, EXIT, NOSET };

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

					template <typename ...ARGS>
					static pointer make_norecord(ARGS&& ...args)noexcept {
						auto ptr = reinterpret_cast<pointer>(malloc(sizeof(value_type)));
						if (ptr == NULL)
							return nullptr;

						try {
							new(ptr) value_type(std::forward<ARGS>(args)...);
						}
						catch (...) {
							free(ptr);
							ptr = nullptr;
						}
						return ptr;
					}

					inline bool put(value_type& obj)noexcept {
						try {
							return m_list.push(&obj);
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

					inline void destory(void* ptr) {
						reinterpret_cast<pointer>(ptr)->~value_type();
						free(ptr);
					}

				private:
					boost::lockfree::queue<pointer> m_list;
				};

				struct IOContext {
					OVERLAPPED m_overlapped;
					OperationType m_type;
					WSABUF m_wsabuf;
					DC::WinSock::Socket m_socket;

					IOContext(const DC::size_t& _Buffer_Size) :m_overlapped{ 0 }, m_type(OperationType::NOSET), m_wsabuf{ 0 } {
						make_buffer(_Buffer_Size);
					}

					IOContext(const IOContext&) = delete;

					~IOContext() {
						if (!isNull(m_wsabuf.buf)) {
							free(m_wsabuf.buf);
							m_wsabuf.buf = nullptr;
							m_wsabuf.len = 0;
						}
					}

					void make_buffer(const DC::size_t& _Buffer_Size) {
						if (_Buffer_Size != 0) {
							m_wsabuf.buf = reinterpret_cast<decltype(WSABUF::buf)>(malloc(_Buffer_Size));
							m_wsabuf.len = _Buffer_Size;
							if (isNull(m_wsabuf.buf)) {
								m_wsabuf.buf = nullptr;
								m_wsabuf.len = 0;
								throw DC::Exception("IOContext::make_buffer", "malloc returned NULL");
							}
							reset_buffer();
						}
						else {
							m_wsabuf.buf = nullptr;
							m_wsabuf.len = 0;
						}
					}

					inline void reset_buffer() {
						if (!isNull(m_wsabuf.buf))
							memset(m_wsabuf.buf, 0, m_wsabuf.len);
					}

					inline void reset_overlapped() {
						memset(&m_overlapped, 0, sizeof(m_overlapped));
					}

					inline bool check_socket()const {
						return m_socket != INVALID_SOCKET;
					}
				};

				class SocketContext {
				public:
					SocketContext() :m_IOContextPool(0), m_socket(INVALID_SOCKET) {
						memset(&m_clientAddress, 0, sizeof(m_clientAddress));
					}

					SocketContext(const SocketContext&) = delete;

					~SocketContext() {
						if (isNull(this)) return;
						close_socket();
						this->m_IOContextPool.clear();
					}

				public:
					void set_socket(const DC::WinSock::Socket& input) {
						m_socket = input;
					}

					DC::WinSock::Socket get_socket()const {
						return m_socket;
					}

					DC::WinSock::Address get_client_address()const {
						return m_clientAddress;
					}

					inline void set_client_address(const DC::WinSock::Address& input) {
						m_clientAddress = input;
					}

					void close_socket() {
						if (m_socket != INVALID_SOCKET) {
							std::lock_guard<SpinLock> lockit(m_socket_lock);
							DC::WinSock::Close(m_socket);
						}
					}

					DC::timer get_timer()const {
						return m_timer;
					}

					inline bool check_socket()const {
						return m_socket != INVALID_SOCKET;
					}

				public:
					template <typename ...ARGS>
					inline IOContext* make_IOContext(ARGS&& ...args) {
						auto temp = m_IOContextPool.make(std::forward<ARGS>(args)...);
						temp->m_socket = this->m_socket;
						return temp;
					}

					void remove_IOContext(IOContext* ptr) {
						m_IOContextPool.remove_if([&ptr](const IOContext& it) {
							return &it == ptr;
						});
					}

					void clear_IOContext() {
						m_IOContextPool.clear();
					}

				private:
					DC::WinSock::Socket m_socket;
					DC::WinSock::Address m_clientAddress;
					DC::timer m_timer;
					SpinLock m_socket_lock;

					QueueAllocator<IOContext> m_IOContextPool;
				};

				struct PSCdeleter {
				public:
					inline void operator()(SocketContext *ptr)const {
						ptr->~SocketContext();
						free(ptr);
					}
				};

				inline bool AssociateWithIOCP(HANDLE IOCP, SocketContext* Socketptr) {
					if (isNull(Socketptr)) return false;
					if (!Socketptr->check_socket()) return false;
					return CreateIoCompletionPort(reinterpret_cast<HANDLE>(Socketptr->get_socket()), IOCP, reinterpret_cast<ULONG_PTR>(Socketptr), 0) != NULL;
				}

				bool PostAccept(const SocketContext& ListenSocket, IOContext* IOptr, LPFN_ACCEPTEX AcceptExFunc) {
					if (isNull(IOptr)) return false;
					if (!IOptr->check_socket()) return false;

					DWORD dwBytes = 0;
					IOptr->m_type = OperationType::ACCEPT;

					DC::WinSock::SocketInitOverlapped(IOptr->m_socket);
					if (!IOptr->check_socket()) {
						DC::WinSock::Close(IOptr->m_socket);
						return false;
					}

					if (FALSE == AcceptExFunc(ListenSocket.get_socket(), IOptr->m_socket, IOptr->m_wsabuf.buf, 0, sizeof(DC::WinSock::Address) + 16, sizeof(DC::WinSock::Address) + 16, &dwBytes, &IOptr->m_overlapped))
						if (WSA_IO_PENDING != WSAGetLastError())
							return false;

					return true;
				}

				bool PostRecv(IOContext* IOptr) {
					if (isNull(IOptr)) return false;
					if (!IOptr->check_socket()) return false;

					DWORD dwBytes = 0, dwFlags = 0;
					IOptr->m_type = OperationType::RECV;

					if (SOCKET_ERROR == WSARecv(IOptr->m_socket, &IOptr->m_wsabuf, 1, &dwBytes, &dwFlags, &IOptr->m_overlapped, 0)) {
						if (WSA_IO_PENDING != WSAGetLastError())
							return false;
					}

					return true;
				}

				bool PostSend(IOContext* IOptr) {
					if (isNull(IOptr)) return false;
					if (!IOptr->check_socket()) return false;
				}

				bool PostExit(IOContext* IOptr) {
					if (isNull(IOptr)) return false;
					if (!IOptr->check_socket()) return false;
				}

				class dc_iocp_server final {
				public:
					dc_iocp_server(const DC::size_t worker_threadnumber, const DC::size_t usercode_threadnumber, const std::string& listenip, const DC::size_t& listenport) :m_iocp(nullptr),
						m_io_tp(worker_threadnumber), m_usercode_tp(usercode_threadnumber), m_listenAddress(DC::WinSock::MakeAddr(listenip, listenport)), m_pscPool(0) {
						this->get_function();
					}

					dc_iocp_server(const dc_iocp_server&) = delete;

					dc_iocp_server& operator=(const dc_iocp_server&) = delete;

				public:
					bool start() {
						m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
						auto look = start_listen(1);
						m_io_tp.async(&dc_iocp_server::worker, this);
						m_io_tp.start();
						return false;
					}

					void stop();

				public:
					void set_client_max_alive_time();

					void set_cleaner_wakeup_time();

					void set_cleaner_max_block_time();

					void set_recv_buffer_size();

					void set_callback_onAccept();

					void set_callback_onRecv();

					void set_callback_onSend();

					void set_callback_onExcept();

				private:
					void worker()noexcept {
						DWORD dwBytesTransfered = 0;
						OVERLAPPED *pOverlapped = nullptr;
						SocketContext *PSC = nullptr;

						while (true) {
							BOOL GQrv = GetQueuedCompletionStatus(m_iocp, &dwBytesTransfered,
								reinterpret_cast<PULONG_PTR>(&PSC), &pOverlapped, INFINITE);

							//PSC为空(一般意味着完成端口已关闭)
							if (isNull(PSC)) break;
							//完成端口已关闭
							if (isNull(m_iocp)) break;
							//遇到了错误
							if (!GQrv) continue;

							IOContext *PIC = CONTAINING_RECORD(pOverlapped, IOContext, m_overlapped);

							//客户端已经断开连接
							if (dwBytesTransfered == 0 && (PIC->m_type == OperationType::SEND || PIC->m_type == OperationType::RECV)) {
								PSC->close_socket();
								continue;
							}
							switch (PIC->m_type) {
							case OperationType::RECV: {
								dosth();
							}break;
							case OperationType::SEND: {
								dosth();
							}break;
							case OperationType::ACCEPT: {
								do_accept(PIC, this->m_GetAcceptExSockAddrs);
							}break;
							case OperationType::EXIT: {
								return;
							}break;
							case OperationType::NOSET: {
								dosth();
							}break;
							}
							dosth();
							//检查是否超时，是则关闭连接
							/*
							if (!isNull(PSC)) {
								if (PSC->get_timer().getms() >= ConnectionTimeOut.load(std::memory_order_acquire)) pSocketContext->CloseSock(); }
								*/
						}
						dosth();
					}

					void cleaner();

				private:
					inline void get_function() {
						DC::WinSock::Socket tempsock = INVALID_SOCKET;
						GUID GuidAcceptEx = WSAID_ACCEPTEX;
						GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
						DWORD dwBytes = 0;

						DC::WinSock::SocketInitTCP(tempsock);

						if (SOCKET_ERROR == WSAIoctl(tempsock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, sizeof(GuidAcceptEx), &this->m_AcceptEx, sizeof(this->m_AcceptEx), &dwBytes, NULL, NULL)) {
							DC::WinSock::Close(tempsock);
							throw DC::Exception("dc_iocp_server::get_function", "can not get the pointer of function AcceptEx");
						}

						if (SOCKET_ERROR == WSAIoctl(tempsock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidGetAcceptExSockAddrs, sizeof(GuidGetAcceptExSockAddrs), &this->m_GetAcceptExSockAddrs, sizeof(this->m_GetAcceptExSockAddrs), &dwBytes, NULL, NULL)) {
							DC::WinSock::Close(tempsock);
							throw DC::Exception("dc_iocp_server::get_function", "can not get the pointer of function GetAcceptExSockAddrs");
						}
					}

					bool start_listen(const DC::size_t& _Post_Size)noexcept {
						if (m_listenSocket.get_socket() != INVALID_SOCKET)
							return false;

						DC::WinSock::Socket listensocket = INVALID_SOCKET;
						DC::WinSock::SocketInitOverlapped(listensocket);
						if (listensocket == INVALID_SOCKET)
							return false;
						m_listenSocket.set_socket(listensocket);

						if (!AssociateWithIOCP(reinterpret_cast<HANDLE>(this->m_iocp), &m_listenSocket)) {
							m_listenSocket.close_socket();
							m_listenSocket.set_socket(INVALID_SOCKET);
							return false;
						}

						if (!DC::WinSock::Bind(m_listenSocket.get_socket(), this->m_listenAddress)) {
							m_listenSocket.close_socket();
							m_listenSocket.set_socket(INVALID_SOCKET);
							return false;
						}

						if (!DC::WinSock::Listen(m_listenSocket.get_socket(), SOMAXCONN)) {
							m_listenSocket.close_socket();
							m_listenSocket.set_socket(INVALID_SOCKET);
							return false;
						}

						for (DC::size_t i = 0; i < _Post_Size; i++) {
							auto AcceptIOptr = m_listenSocket.make_IOContext(64);
							if (!PostAccept(this->m_listenSocket, AcceptIOptr, this->m_AcceptEx)) {
								m_listenSocket.clear_IOContext();
								return false;
							}
						}

						return true;
					}

					void stop_listen() {

					}

					void do_accept(IOContext* IOptr, LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockAddrsFunc) {
						DC::WinSock::Address *ClientAddress(nullptr), *ServerAddress(nullptr);
						int ClientLen(sizeof(DC::WinSock::Address)), ServerLen(sizeof(DC::WinSock::Address));

						GetAcceptExSockAddrsFunc(IOptr->m_wsabuf.buf, 0, sizeof(DC::WinSock::Address) + 16, sizeof(DC::WinSock::Address) + 16, reinterpret_cast<LPSOCKADDR*>(&ServerAddress), &ServerLen, reinterpret_cast<LPSOCKADDR*>(&ClientAddress), &ClientLen);
						///*
						std::string look = inet_ntoa(ClientAddress->sin_addr);						
						std::cout << "新的客户端:" << look << "\n";
						//*/


						std::unique_ptr<SocketContext, PSCdeleter> NewClientContext(QueueAllocator<SocketContext>::make_norecord());
						if (isNull(NewClientContext.get())) {
							DC::WinSock::Close(IOptr->m_socket);
							return;
						}
						NewClientContext->set_socket(IOptr->m_socket);
						NewClientContext->set_client_address(*ClientAddress);

						if (!AssociateWithIOCP(this->m_iocp, NewClientContext.get()))
							return;

						if (!PostRecv(NewClientContext->make_IOContext(TEMPBUFFERSIZE)))
							return;

						m_pscPool.put(*NewClientContext.release());

						IOptr->reset_buffer();
						PostAccept(this->m_listenSocket, IOptr, this->m_AcceptEx);
					}

				private:
					LPFN_ACCEPTEX m_AcceptEx;//AcceptEx
					LPFN_GETACCEPTEXSOCKADDRS m_GetAcceptExSockAddrs;//GetAcceptExSockaddrs

					HANDLE m_iocp;
					SocketContext m_listenSocket;
					DC::ThreadPool m_io_tp, m_usercode_tp;

					DC::WinSock::Address m_listenAddress;

					QueueAllocator<SocketContext> m_pscPool;
				};

			}

			using Server = IOCPSpace::dc_iocp_server;

		}

	}

}

#endif