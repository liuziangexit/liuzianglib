#pragma once
#ifndef liuzianglib_IOCP
#define liuzianglib_IOCP
#include <winsock2.h>
#include <MSWSock.h>
#include <vector>
#include "liuzianglib.h"
#include "DC_WinSock.h"
#include "DC_ThreadPool.h"
#pragma comment(lib,"ws2_32.lib")
//Version 2.4.2V18
//20170420

namespace DC {

	namespace IOCP {

		namespace IOCPSpace {

			class unique_id {
			public:
				unique_id() :uniqueid(DC::randomer(0, 131070)) {}

				unique_id(const int32_t input) :uniqueid(input) {}

				unique_id(const unique_id&) = default;

			public:
				bool operator==(const unique_id& input)const {
					if (uniqueid == input.uniqueid) return true;
					return false;
				}

			public:
				inline int32_t getUniqueID()const {
					return uniqueid;
				}

			private:
				const int32_t uniqueid;
			};

		}

		enum OperationType { ACCEPT_POSTED, SEND_POSTED, RECV_POSTED, NULL_POSTED };

		struct PerIOContext {//只能在堆上构造，使用destroy成员函数来释放内存
			OVERLAPPED m_overlapped;
			OperationType m_opType;
			SOCKET m_sock;
			WSABUF m_wsabuf;
			IOCPSpace::unique_id uniqueid;

		public:
			PerIOContext(const std::size_t& buffersize) {
				memset(&m_overlapped, NULL, sizeof(m_overlapped));
				m_sock = INVALID_SOCKET;
				m_opType = OperationType::NULL_POSTED;

				m_wsabuf.len = buffersize;
				if (buffersize != 0) {
					m_wsabuf.buf = new char[buffersize];
					resetBuffer();
				}
				else m_wsabuf.buf = nullptr;
			}

			PerIOContext(const PerIOContext& input) = delete;

		public:
			bool operator==(const PerIOContext& input)const {
				if (input.uniqueid == uniqueid) return true;
				return false;
			}

		private:
			~PerIOContext() {}

		public:
			inline void resetBuffer() {
				ZeroMemory(m_wsabuf.buf, m_wsabuf.len);
			}

			inline void CloseSock() {
				if (m_sock != INVALID_SOCKET) {
					closesocket(m_sock);
					m_sock = INVALID_SOCKET;
				}
			}

			void destroy() {
				CloseSock();

				if (m_wsabuf.buf != nullptr) {
					delete[] m_wsabuf.buf;
				}
				delete this;
			}
		};

		class PerSocketContext;

		namespace IOCPSpace {

			struct PICdeleter {
			public:
				inline void operator()(IOCP::PerIOContext *ptr) {
					ptr->destroy();
				}
			};

			template <typename T, typename deleter = void>
			class Pool {
				friend class PerSocketContext;
			public:
				Pool() = default;

				virtual ~Pool() = default;

			public:
				template <typename ...U>
				inline T* make(U&& ...args)noexcept {
					try {
						m.emplace_back(new T(std::forward<U>(args)...));
					}
					catch (...) {
						return nullptr;
					}
					return m.rbegin()->get();
				}

				template <typename ...U>
				inline T* put(U&& ...args)noexcept {//only unique_ptr
					try {
						m.emplace_back(std::forward<U>(args)...);
					}
					catch (...) {
						return nullptr;
					}
					return m.rbegin()->get();
				}

				T* release(const T *input) {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (*input == *(i->get()))
							return i->release();
					}
					return nullptr;
				}

				bool drop(const T *input) {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (*input == *(i->get())) {
							i->get_deleter()(i->release());
							return true;
						}
					}
					return false;
				}

				void clean() {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (!(*i)) { m.erase(i); clean(); break; }
					}
				}

				inline void clear() {
					m.clear();
				}

			private:				
				std::vector<std::unique_ptr<T, deleter>> m;
			};

			template <typename T>
			class Pool<T, void> {
			public:
				Pool() = default;

			public:
				template <typename ...U>
				inline T* make(U&& ...args)noexcept {
					try {
						m.emplace_back(new T(std::forward<U>(args)...));
					}
					catch (...) {
						return nullptr;
					}
					return m.rbegin()->get();
				}

				template <typename ...U>
				inline T* put(U&& ...args)noexcept {//only unique_ptr
					try {
						m.emplace_back(std::forward<U>(args)...);
					}
					catch (...) {
						return nullptr;
					}
					return m.rbegin()->get();
				}

				T* release(const T *input) {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (*input == *(i->get()))
							return i->release();
					}
					return nullptr;
				}

				bool drop(const T *input) {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (*input == *(i->get())) {
							i->get_deleter()(i->release());
							return true;
						}
					}
					return false;
				}

				void clean() {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (!(*i)) { m.erase(i); clean(); break; }
					}
				}

				inline void clear() {
					m.clear();
				}

			private:
				std::vector<std::unique_ptr<T>> m;
			};

		}

		class PerSocketContext :public IOCPSpace::Pool<PerIOContext, IOCPSpace::PICdeleter> {
		public:
			PerSocketContext() {
				m_sock = INVALID_SOCKET;
			}

			~PerSocketContext() {
				CloseSock();
			}

		public:
			bool operator==(const PerSocketContext& input)const {
				if (input.uniqueid == uniqueid) return true;
				return false;
			}

		public:
			inline void CloseSock() {
				if (m_sock != INVALID_SOCKET) {
					closesocket(m_sock);
					m_sock = INVALID_SOCKET;
				}
			}

		public:
			SOCKET m_sock;
			SOCKADDR_IN m_clientAddr;
			IOCPSpace::unique_id uniqueid;
		};

		using PICptr = std::unique_ptr<PerIOContext, IOCPSpace::PICdeleter>;
		using PSCptr = std::unique_ptr<PerSocketContext>;

		class Server {
		public:
			Server(const std::size_t& inputThreadNumber) :TP(nullptr), ThreadNumber(inputThreadNumber), m_listen(INVALID_SOCKET), m_iocp(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)), AcceptEx_p(nullptr), GetAcceptExSockAddrs_p(nullptr) {
				//线程数量+1是为了给accept线程留一个
				if (m_iocp == NULL) throw DC::DC_ERROR("Server::Server", "CreateIoCompletionPort error", -1);

				GUID GuidAcceptEx = WSAID_ACCEPTEX, GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
				PICptr holder(new PerIOContext(1));
				DC::WinSock::SocketInit_TCP(holder->m_sock);
				DWORD dwBytes = 0;
				if (SOCKET_ERROR == WSAIoctl(holder->m_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, sizeof(GuidAcceptEx), &AcceptEx_p, sizeof(AcceptEx_p), &dwBytes, NULL, NULL) ||
					SOCKET_ERROR == WSAIoctl(holder->m_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidGetAcceptExSockAddrs, sizeof(GuidGetAcceptExSockAddrs), &GetAcceptExSockAddrs_p, sizeof(GetAcceptExSockAddrs_p), &dwBytes, NULL, NULL))
					throw DC::DC_ERROR("Server::Server", "can't get function pointer", -1);
			}

			virtual ~Server()noexcept {
				Stop();
				if (m_iocp != nullptr&&m_iocp != NULL) CloseHandle(m_iocp);
			}

		public:
			static inline bool LoadSocketLib() {
				return WinSock::Startup(2, 2);
			}

			static inline void UnloadSocketLib() {
				WinSock::Cleanup();
			}

			inline void SetListenAddr(const WinSock::Address& input) {
				bindAddr = input;
			}

			inline void SetListenAddr(const std::string& ip, const int32_t& port) {
				bindAddr = WinSock::MakeAddr(ip, port);
			}

			void Start() {
				TP = new DC::ThreadPool(ThreadNumber + 1);
				TP->async(&Server::ListenThread, this);
				for (std::size_t i = 0; i < ThreadNumber; i++)
					TP->async(&Server::WorkerThread, this);
				TP->start();
			}

			inline void Stop() {
				PICptr PIC(new PerIOContext(1));
				closesocket(m_listen);
				PostExit(PIC.get());
				if (TP != nullptr) { delete TP; TP = nullptr; }
				PSC_Pool.clear();		
			}

			virtual void OnRecv(PerSocketContext *PSC, const std::string& recvstr, const DC::WinSock::Address& ClientAddr) {}

			virtual void OnSend(PerSocketContext *PSC, const std::string& sendstr, const DC::WinSock::Address& ClientAddr) {}

		protected:
			virtual inline const std::size_t getRecvBufferSize() {//recv 缓冲区
				return 1024;
			}

			virtual inline const std::size_t getSendBufferSize() {//send 缓冲区
				return 1024;
			}

		private:
			bool PostRecv(PerSocketContext *PSC, PerIOContext *PIC) {
				DWORD dwFlags = 0;
				DWORD dwBytes = 0;

				PIC->resetBuffer();
				PIC->m_opType = RECV_POSTED;

				int nBytesRecv = WSARecv(PSC->m_sock, &PIC->m_wsabuf, 1, &dwBytes, &dwFlags, &PIC->m_overlapped, NULL);

				if (SOCKET_ERROR == nBytesRecv) {
					if (WSA_IO_PENDING != WSAGetLastError())
						return false;
				}
				return true;
			}

			inline bool DoRecv(PerSocketContext *PSC, PerIOContext *PIC) {
				OnRecv(PSC, PIC->m_wsabuf.buf, PSC->m_clientAddr);
				return PostRecv(PSC, PIC);
			}

			bool PostSend(PerSocketContext *PSC, PerIOContext *PIC, const std::string& sendthis) {
				DWORD dwFlags = 0;
				DWORD dwBytes = sendthis.size();

				if (sendthis.size() > PIC->m_wsabuf.len) return false;

				PIC->resetBuffer();
				memcpy(PIC->m_wsabuf.buf, sendthis.c_str(), sendthis.size());
				PIC->m_opType = SEND_POSTED;

				int nBytesRecv = WSASend(PSC->m_sock, &PIC->m_wsabuf, 1, &dwBytes, dwFlags, &PIC->m_overlapped, NULL);

				if (SOCKET_ERROR == nBytesRecv) {
					if (WSA_IO_PENDING != WSAGetLastError())
						return false;
				}
				return true;
			}

			inline bool Send(PerSocketContext *client, const std::string& sendthis) {
				return PostSend(client, client->make(getSendBufferSize()), sendthis);
			}

			inline bool DoSend(PerSocketContext *PSC, PerIOContext *PIC) {
				OnSend(PSC, PIC->m_wsabuf.buf, PSC->m_clientAddr);
				if (!PSC->drop(PIC)) throw DC_ERROR("DoSend", "", -1);
			}

			inline bool PostExit(PerIOContext *PIC) {
				PerIOContext *AcceptIoContext = PIC;
				DWORD dwBytes = 0;
				AcceptIoContext->m_opType = OperationType::NULL_POSTED;

				if (0 != PostQueuedCompletionStatus(m_iocp, dwBytes, reinterpret_cast<ULONG_PTR>(AcceptIoContext), &AcceptIoContext->m_overlapped)) return true;
				return false;
			}

			void WorkerThread() {				
				DWORD dwBytesTransfered = 0;
				OVERLAPPED *pOverlapped = NULL;
				PerSocketContext *pSocketContext = NULL;

				while (true) {
					BOOL GQrv = GetQueuedCompletionStatus(m_iocp, &dwBytesTransfered,
						reinterpret_cast<PULONG_PTR>(&pSocketContext), &pOverlapped, INFINITE);

					if (NULL == pSocketContext) break;
					if (!GQrv) {//遇到了错误
						continue;
					}

					PerIOContext *pIOContext = CONTAINING_RECORD(pOverlapped, PerIOContext, m_overlapped);

					if (dwBytesTransfered == 0 && (pIOContext->m_opType == OperationType::RECV_POSTED || pIOContext->m_opType == OperationType::RECV_POSTED)) {
						pSocketContext->CloseSock();
						continue;
					}

					switch (pIOContext->m_opType) {
					case OperationType::ACCEPT_POSTED: {}break;
					case OperationType::RECV_POSTED: {
						DoRecv(pSocketContext, pIOContext);
					}break;
					case OperationType::SEND_POSTED: {
						DoSend(pSocketContext, pIOContext);
					}break;
					case OperationType::NULL_POSTED: {return; }break;
					}
				}
			}

			void ListenThread()noexcept {
				WinSock::SocketInit_TCP(m_listen);
				if (m_listen == SOCKET_ERROR) return;

				if (!WinSock::Bind(m_listen, bindAddr)) { closesocket(m_listen); return; }				
				if (!WinSock::Listen(m_listen, SOMAXCONN)) { closesocket(m_listen); return; }

				SOCKET acceptSocket;
				DC::WinSock::Address ClientAddr;

				while (true) {
					
					if (!DC::WinSock::Accept(acceptSocket, m_listen,ClientAddr)) { closesocket(m_listen); return; }

					PSCptr ptr(new PerSocketContext);
					ptr->m_sock = acceptSocket;
					ptr->m_clientAddr = ClientAddr;
					if (!AssociateWithIOCP(ptr.get())) continue;
					if (!PostRecv(ptr.get(), ptr->make(getRecvBufferSize()))) continue;
					PSC_Pool.put(std::move(ptr));
				}
			}

			inline bool AssociateWithIOCP(PerSocketContext* input) {
				HANDLE hTemp = CreateIoCompletionPort(reinterpret_cast<HANDLE>(input->m_sock), m_iocp, reinterpret_cast<DWORD>(input), 0);

				if (NULL == hTemp)
					return false;
				return true;
			}

		protected:
			HANDLE m_iocp;
			SOCKET m_listen;
			ThreadPool *TP;
			const std::size_t ThreadNumber;

			WinSock::Address bindAddr;

			IOCPSpace::Pool<PerSocketContext> PSC_Pool;

			LPFN_ACCEPTEX AcceptEx_p;
			LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockAddrs_p;
		};
		
	}

}

#endif