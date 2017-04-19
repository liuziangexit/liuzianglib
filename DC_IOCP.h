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
//Version 2.4.2V17
//20170419

namespace DC {

	namespace IOCP {

		namespace IOCPSpace {

			class unique_id {
			public:
				unique_id() :uniqueid(DC::randomer(0, 131070)) {}

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
					if (buffersize != 1) resetBuffer(); else m_wsabuf.buf[0] = NULL;
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
				memset(m_wsabuf.buf, NULL, m_wsabuf.len);
			}

			inline void CloseSock() {
				if (m_sock != INVALID_SOCKET) {
					closesocket(m_sock);
					m_sock = INVALID_SOCKET;
				}
			}

			PerIOContext* Clone() {
				auto rv = new(std::nothrow) PerIOContext(m_wsabuf.len);
				rv->m_opType = m_opType;
				rv->m_overlapped = m_overlapped;
				rv->m_sock = m_sock;
				memcpy(rv->m_wsabuf.buf, m_wsabuf.buf, m_wsabuf.len);
				return rv;
			}

			void destroy() {
				CloseSock();

				if (m_wsabuf.buf != nullptr) delete[] m_wsabuf.buf;
				delete this;
			}
		};

		class PerSocketContext;

		namespace IOCPSpace {

			struct PICdeleter {
			public:
				inline void operator()(IOCP::PerIOContext *ptr) {
					closesocket(ptr->m_sock);
					ptr->destroy();
				}
			};

			template <typename T, typename deleter = void>
			class Pool {
				friend class PerSocketContext;
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

				T* put(T* ptr) {
					m.emplace_back();
					m.rbegin()->reset(ptr);
					return ptr;
				}

				bool release(const T& input) {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (input == *(i->get())) {
							i->release();
							m.erase(i);
							return true;
						}
					}
					return false;
				}

				bool drop(const T& input) {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (input == *(i->get())) { m.erase(i); return true; }
					}
					return false;
				}

				bool inside(const T& input)const {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (input == *(i->get())) return true;
					}
					return false;
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

				T* put(T* ptr) {
					m.emplace_back();
					m.rbegin()->reset(ptr);
					return ptr;
				}

				bool release(const T& input) {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (input == *(i->get())) {
							i->release();
							m.erase(i);
							return true;
						}
					}
					return false;
				}

				bool drop(const T& input) {
					for (auto i = m.begin(); i != m.end(); i++) {
						if (input == *(i->get())) {
							m.erase(i);
							return true;
						}
					}
					return false;
				}

				inline void clear() {
					m.clear();
				}

			private:
				std::vector<std::unique_ptr<T>> m;
			};

		}

		class PerSocketContext {
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

			PerSocketContext* Clone() {
				auto rv = new PerSocketContext;
				rv->m_sock = m_sock;
				rv->m_clientAddr = m_clientAddr;
				return rv;
			}

			inline PerIOContext* make(const std::size_t& input)noexcept {
				return m_listIOContext.make(input);
			}

			inline bool drop(const PerIOContext& input) {
				return m_listIOContext.drop(input);
			}

		public:
			SOCKET m_sock;
			SOCKADDR_IN m_clientAddr;
			IOCPSpace::Pool<PerIOContext, IOCPSpace::PICdeleter> m_listIOContext;
			IOCPSpace::unique_id uniqueid;
		};

		class Server {
		public:
			Server(const std::size_t& inputThreadNumber) :TP(inputThreadNumber), ThreadNumber(inputThreadNumber), m_iocp(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)), m_listen(nullptr), MaxAcceptEx(1), AcceptEx_p(nullptr), GetAcceptExSockAddrs_p(nullptr) {
				if (m_iocp == NULL) throw DC::DC_ERROR("Server::Server", "CreateIoCompletionPort error", -1);

				GUID GuidAcceptEx = WSAID_ACCEPTEX, GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
				auto holder = PIC_Pool.make(1);
				DC::WinSock::SocketInit_TCP(holder->m_sock);
				DWORD dwBytes = 0;
				if (SOCKET_ERROR == WSAIoctl(holder->m_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, sizeof(GuidAcceptEx), &AcceptEx_p, sizeof(AcceptEx_p), &dwBytes, NULL, NULL) ||
					SOCKET_ERROR == WSAIoctl(holder->m_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidGetAcceptExSockAddrs, sizeof(GuidGetAcceptExSockAddrs), &GetAcceptExSockAddrs_p, sizeof(GetAcceptExSockAddrs_p), &dwBytes, NULL, NULL))
					throw DC::DC_ERROR("Server::Server", "can't get function pointer", -1);
				PIC_Pool.drop(*holder);
				TP.start();
			}

			virtual ~Server()noexcept {
				Stop();
				PostExit();
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

			inline std::size_t& MaxAcceptExNumber() {
				return MaxAcceptEx;
			}

			bool Start() {
				if (!startListen()) return false;
				
				for (std::size_t i = 0; i < ThreadNumber; i++)
					TP.async(&Server::WorkerThread, this);
				return true;
			}

			inline void Stop() {
				PostExit();
				if (m_listen != nullptr&&m_listen != NULL) delete m_listen;
				m_listen = nullptr;
				PIC_Pool.clear();
				PSC_Pool.clear();
			}

		public:
			bool PostAccept() {
				if (m_listen == nullptr || m_listen == NULL) return false;
				if (AcceptEx_p == nullptr || AcceptEx_p == NULL) return false;
				if (m_listen->m_sock == INVALID_SOCKET) return false;

				PerIOContext *AcceptIoContext = PIC_Pool.make(getBufferSize());
				DWORD dwBytes = 0;
				AcceptIoContext->m_opType = ACCEPT_POSTED;
				if ((AcceptIoContext->m_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) return false;

				//注意，AcceptIoContext->m_wsabuf.buf 不能是 null
				if (!AcceptEx_p(m_listen->m_sock, AcceptIoContext->m_sock, AcceptIoContext->m_wsabuf.buf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &dwBytes, &AcceptIoContext->m_overlapped)) {
					if (WSAGetLastError() == ERROR_IO_PENDING) return true;
					return false;
				}
				return true;
			}

			bool PostAccept(PerIOContext *PIC) {
				if (m_listen == nullptr || m_listen == NULL) return false;
				if (AcceptEx_p == nullptr || AcceptEx_p == NULL) return false;
				if (m_listen->m_sock == INVALID_SOCKET) return false;
				if (!PIC_Pool.inside(*PIC)) return false;

				PerIOContext *AcceptIoContext = PIC;
				DWORD dwBytes = 0;
				AcceptIoContext->m_opType = ACCEPT_POSTED;
				if ((AcceptIoContext->m_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) return false;

				//注意，AcceptIoContext->m_wsabuf.buf 不能是 null
				if (!AcceptEx_p(m_listen->m_sock, AcceptIoContext->m_sock, AcceptIoContext->m_wsabuf.buf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &dwBytes, &AcceptIoContext->m_overlapped)) {
					if (WSAGetLastError() == ERROR_IO_PENDING) return true;
					return false;
				}
				return true;
			}

			inline bool PostExit() {
				PerIOContext *AcceptIoContext = PIC_Pool.make(getBufferSize());
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
					case OperationType::ACCEPT_POSTED: {
						std::cout << "got accept\n";
						if (!DoAccept(pSocketContext, pIOContext))
							PostAccept();
					}break;
					case OperationType::RECV_POSTED: {}break;
					case OperationType::SEND_POSTED: {}break;
					case OperationType::NULL_POSTED: {return; }break;
					default: {
						throw std::exception("some motherfuckers fucked up");
					}break;
					}
				}
			}

		protected:
			virtual inline const std::size_t getBufferSize() {
				return 1;
			}

			bool DoAccept(PerSocketContext *PSC, PerIOContext *PIC) {
				SOCKADDR_IN* ClientAddr = NULL;
				SOCKADDR_IN* LocalAddr = NULL;
				int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);

				GetAcceptExSockAddrs_p(PIC->m_wsabuf.buf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);

				auto temp = PSC_Pool.make();
				temp->m_sock = PIC->m_sock;
				temp->m_clientAddr = *ClientAddr;
				if (!AssociateWithIOCP(temp)) {
					if (!PSC_Pool.drop(*temp)) throw DC::DC_ERROR("DoAccept", "", -1);
					return false; 
				}
				
				auto temp2 = temp->make(getBufferSize());
				temp2->m_opType = OperationType::RECV_POSTED;
				temp2->m_sock = temp->m_sock;
				if (true) {//投递recv
					if (!PSC_Pool.drop(*temp)) throw DC::DC_ERROR("DoAccept", "", -1);
					return false;
				}

				PIC->resetBuffer();
				return PostAccept(PIC);
			}

		private:
			bool startListen()noexcept {
				if (m_listen != nullptr || m_listen != NULL) return false;

				PerSocketContext *ptr = PSC_Pool.make();
				if (ptr == nullptr) { PSC_Pool.drop(*ptr); return false; }

				WinSock::SocketInitOverlapped(ptr->m_sock);
				if (ptr->m_sock == SOCKET_ERROR) { PSC_Pool.drop(*ptr); return false; }

				if (!AssociateWithIOCP(ptr)) { PSC_Pool.drop(*ptr); return false; }

				if (!WinSock::Bind(ptr->m_sock, bindAddr)) { PSC_Pool.drop(*ptr); return false; }
				if (!WinSock::Listen(ptr->m_sock, SOMAXCONN)) { PSC_Pool.drop(*ptr); return false; }

				m_listen = ptr;
				if (!PSC_Pool.release(*ptr)) throw DC::DC_ERROR("startListen", "some asshole got really fucked up.this is the fucking undefined behavior,i dont know why this happend,but fuck this anyway", -1);//严重错误

				for (std::size_t i = 0; i < MaxAcceptEx; i++) {
					if (!PostAccept()) return false;
				}
				return true;
			}

			inline bool AssociateWithIOCP(PerSocketContext* input) {
				HANDLE hTemp = CreateIoCompletionPort(reinterpret_cast<HANDLE>(input->m_sock), m_iocp, reinterpret_cast<DWORD>(input), 0);

				if (NULL == hTemp)
					return false;
				return true;
			}

		protected:
			HANDLE m_iocp;
			PerSocketContext *m_listen;
			ThreadPool TP;
			std::size_t MaxAcceptEx;
			const std::size_t ThreadNumber;

			WinSock::Address bindAddr;

			IOCPSpace::Pool<PerIOContext, IOCPSpace::PICdeleter> PIC_Pool;
			IOCPSpace::Pool<PerSocketContext> PSC_Pool;

			LPFN_ACCEPTEX AcceptEx_p;
			LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockAddrs_p;
		};

	}

}

#endif