#pragma once
#ifndef liuzianglib_IOCP
#define liuzianglib_IOCP
#include <winsock2.h>
#include <MSWSock.h>
#include <vector>
#include "liuzianglib.h"
#include "DC_WinSock.h"
#include "DC_ThreadPool.h"
#include "DC_timer.h"
#pragma comment(lib,"ws2_32.lib")
//Version 2.4.2V19
//20170421

namespace DC {

	namespace IOCP {

		class Server;

		namespace IOCPSpace {

			class unique_id {
			public:
				unique_id() :uniqueid(DC::randomer(0, 21476836476)) {}

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
				Pool() :removeTime(0), cleanLimit(1) {}

				virtual ~Pool() = default;

			public:
				inline void setCleanLimit(const int32_t& input) {
					if (input < 1) { cleanLimit.store(1, std::memory_order_release); return; }
					cleanLimit.store(input, std::memory_order_release);
				}

				template <typename ...U>
				inline T* make(U&& ...args)noexcept {
					Cleanif();
					std::unique_lock<std::mutex> LwriteLock(writeLock);
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
					Cleanif();
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					try {
						m.emplace_back(std::forward<U>(args)...);
					}
					catch (...) {
						return nullptr;
					}
					return m.rbegin()->get();
				}

				T* release(const T *input) {
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					for (auto i = m.begin(); i != m.end(); i++) {
						if (*input == *(i->get())) {
							removeTime++;
							return i->release();
						}
					}
					return nullptr;
				}

				bool drop(const T *input) {
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					auto fres = std::find_if(m.begin(), m.end(), std::bind(&Pool::compare, this, std::placeholders::_1, input));

					if (fres == m.end())
						return false;

					removeTime++;
					fres->get_deleter()(fres->release());
					return true;
				}

				void clean() {
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					realClean();
					removeTime.store(0, std::memory_order_release);
				}

				inline void clear() {
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					m.clear();
					removeTime.store(0, std::memory_order_release);
				}

			private:
				void realClean() {
					for (auto i = m.begin(); i != m.end();) {
						if (!(*i)) { i = m.erase(i); continue; }
						i++;
					}
				}

				inline void Cleanif() {
					if (removeTime.load(std::memory_order_acquire) >= cleanLimit.load(std::memory_order_acquire)) {
						clean();
					}
				}

				inline bool compare(const std::unique_ptr<T, deleter>& target, const T* const value) {
					if (target.get() == value) return true;
					return false;
				}

			private:
				std::vector<std::unique_ptr<T, deleter>> m;
				std::mutex writeLock;
				std::atomic_int32_t removeTime, cleanLimit;
			};

			template <typename T>
			class Pool<T, void> {
				friend class Server;
			public:
				Pool() :removeTime(0), cleanLimit(1) {}

			public:
				inline void setCleanLimit(const int32_t& input) {
					if (input < 1) { cleanLimit.store(1, std::memory_order_release); return; }
					cleanLimit.store(input, std::memory_order_release);
				}

				template <typename ...U>
				inline T* make(U&& ...args)noexcept {
					Cleanif();
					std::unique_lock<std::mutex> LwriteLock(writeLock);
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
					Cleanif();
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					try {
						m.emplace_back(std::forward<U>(args)...);
					}
					catch (...) {
						return nullptr;
					}
					return m.rbegin()->get();
				}

				T* release(const T *input) {
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					for (auto i = m.begin(); i != m.end(); i++) {
						if (*input == *(i->get())) {
							removeTime++;
							return i->release();
						}
					}
					return nullptr;
				}

				bool drop(const T *input) {
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					auto fres = std::find_if(m.begin(), m.end(), std::bind(&Pool::compare, this, std::placeholders::_1, input));

					if (fres == m.end())
						return false;

					removeTime++;
					fres->get_deleter()(fres->release());
					return true;
				}

				void clean() {
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					realClean();
					removeTime.store(0, std::memory_order_release);
				}

				inline void clear() {
					std::unique_lock<std::mutex> LwriteLock(writeLock);
					m.clear();
					removeTime.store(0, std::memory_order_release);
				}

			private:
				void realClean() {
					for (auto i = m.begin(); i != m.end();) {
						if (!(*i)) { i = m.erase(i); continue; }
						i++;
					}
				}

				inline void Cleanif() {
					if (removeTime.load(std::memory_order_acquire) >= cleanLimit.load(std::memory_order_acquire)) {
						clean();
					}
				}

				inline bool compare(const std::unique_ptr<T>& target, const T* const value) {
					if (target.get() == value) return true;
					return false;
				}

				bool drop_nolock(const T *input) {
					auto fres = std::find_if(m.begin(), m.end(), std::bind(&Pool::compare, this, std::placeholders::_1, input));

					if (fres == m.end())
						return false;

					removeTime++;
					fres->get_deleter()(fres->release());
					return true;
				}

			private:
				std::vector<std::unique_ptr<T>> m;
				std::mutex writeLock;
				std::atomic_int32_t removeTime, cleanLimit;
			};

		}

		class PerSocketContext :public IOCPSpace::Pool<PerIOContext, IOCPSpace::PICdeleter> {
		public:
			PerSocketContext(const int32_t& PoolCleanLimit) {
				setCleanLimit(PoolCleanLimit);
				m_sock = INVALID_SOCKET;
				timer.start();
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

			inline time_t getTimerSeconds() {
				std::unique_lock<std::mutex> LtimerM(timerM);
				return timer.getsecond();
			}

		public:
			SOCKET m_sock;
			SOCKADDR_IN m_clientAddr;
			IOCPSpace::unique_id uniqueid;
		private:
			std::mutex timerM;
			DC::timer timer;
		};

		using PICptr = std::unique_ptr<PerIOContext, IOCPSpace::PICdeleter>;
		using PSCptr = std::unique_ptr<PerSocketContext>;

		class Server {
		public:
			Server(const std::size_t& inputThreadNumber) :TP(nullptr), ThreadNumber(inputThreadNumber), m_listen(INVALID_SOCKET), m_iocp(nullptr), PoolCleanLimit(1), CleanTimeInterval(1), stopFlag(true) {}

			Server(const Server&) = delete;

			virtual ~Server()noexcept {
				Stop();				
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

			inline void SetMemoryPoolCleanLimit(const int32_t& input) {
				PoolCleanLimit.store(input, std::memory_order_release);
				PSC_Pool.setCleanLimit(input);
			}

			inline void SetCleanTimeInterval(const int32_t& input) {
				CleanTimeInterval.store(input, std::memory_order_release);
			}

			inline void SetConnectionMaxLiveTime(const int32_t& input) {
				ConnectionTimeOut.store(input, std::memory_order_release);
			}

			bool Start(int32_t waitlimit) {
				m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
				if (m_iocp == NULL || m_iocp == nullptr) return false;
				if (waitlimit < 1) waitlimit = 1;
				TP = new DC::ThreadPool(ThreadNumber + 2);//cleaner和listener
				TP->start();
				
				auto wait = TP->async(&Server::ListenerThread, this);
				if (wait.wait_for(std::chrono::seconds(waitlimit)) != std::future_status::timeout)
					return false;

				TP->async(&Server::CleanerThread, this, std::chrono::seconds(CleanTimeInterval.load(std::memory_order_acquire)));

				for (std::size_t i = 0; i < ThreadNumber; i++)
					TP->async(&Server::WorkerThread, this);

				stopFlag.store(false, std::memory_order_release);
				return true;
			}

			inline void Stop() {
				//停止监听
				closesocket(m_listen);
				//向清洁线程发送停止信号
				stopFlag.store(true, std::memory_order_release);
				cleanerCV.notify_all();
				//向工作线程发送停止信号
				std::vector<DC::IOCP::PICptr> temp;
				for (int i = 0; i < ThreadNumber; i++)
					temp.emplace_back(new DC::IOCP::PerIOContext(getSendBufferSize()));
				for (auto& p : temp)
					PostExit(p.get());
				//关闭完成端口
				if (m_iocp != nullptr&&m_iocp != NULL) { CloseHandle(m_iocp); m_iocp = nullptr; }
				//关闭并删除线程池
				if (TP != nullptr) { delete TP; TP = nullptr; }
				//清空客户端SOCKET
				PSC_Pool.clear();
			}

			static inline void loop() {//永远不会返回
				while (true)
					std::this_thread::sleep_for(std::chrono::minutes(9710));
			}

		protected:
			virtual inline const std::size_t getRecvBufferSize()noexcept {//recv 缓冲区
				return 1024;
			}

			virtual inline const std::size_t getSendBufferSize()noexcept {//send 缓冲区
				return 1024;
			}

			virtual void OnRecv(DC::IOCP::PerSocketContext *PSC, const std::string& recvstr, const DC::WinSock::Address& ClientAddr) {}

			virtual void OnSend(DC::IOCP::PerSocketContext *PSC, const std::string& sendstr, const DC::WinSock::Address& ClientAddr) {}

			inline bool Send(PerSocketContext *client, const std::string& sendthis) {
				return PostSend(client, client->make(getSendBufferSize()), sendthis);
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

					if (dwBytesTransfered == 0 && (pIOContext->m_opType == OperationType::SEND_POSTED || pIOContext->m_opType == OperationType::RECV_POSTED)) {
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

			bool ListenerThread()noexcept {
				WinSock::SocketInit_TCP(m_listen);
				if (m_listen == SOCKET_ERROR) return false;

				if (!WinSock::Bind(m_listen, bindAddr)) { closesocket(m_listen); return false; }
				if (!WinSock::Listen(m_listen, SOMAXCONN)) { closesocket(m_listen); return false; }

				SOCKET acceptSocket;
				DC::WinSock::Address ClientAddr;

				while (true) {
					acceptSocket = INVALID_SOCKET;
					if (!DC::WinSock::Accept(acceptSocket, m_listen, ClientAddr)) { closesocket(m_listen); return false; }

					PSCptr ptr(new PerSocketContext(PoolCleanLimit.load(std::memory_order_acquire)));
					ptr->m_sock = acceptSocket;
					ptr->m_clientAddr = ClientAddr;
					if (!AssociateWithIOCP(ptr.get())) continue;
					if (!PostRecv(ptr.get(), ptr->make(getRecvBufferSize()))) continue;
					PSC_Pool.put(std::move(ptr));
				}

				return true;
			}

			void CleanerThread(const std::chrono::seconds limits) {
				//std::cout << "cleaner开始\n";
				DC::timer timer;
				std::chrono::seconds templimits;
				while (true) {
					templimits = limits;
					timer.reset();
					timer.start();
					//std::cout << "cleaner检查\n";
					std::unique_lock<std::mutex> lock(cleanerMut);
					while (true) {
						if (cleanerCV.wait_for(lock, templimits) == std::cv_status::timeout) 
							if (stopFlag == true)
							{
								//std::cout << "cleaner退出\n";
								return;
							}
							else//工作
								break;
						timer.stop();
						if (std::chrono::seconds(timer.getsecond()) >= templimits)
							if (stopFlag == true)//真唤醒
							{
								//std::cout << "cleaner退出\n";
								return;
							}
							else//假唤醒
								break;
						//假唤醒
						templimits = templimits - std::chrono::seconds(timer.getsecond());
						timer.reset();
						timer.start();
					}
					//std::cout << "cleaner工作\n";

					std::unique_lock<std::mutex> lockPSC(PSC_Pool.writeLock);
					for (auto& p : PSC_Pool.m) {
						if (p.get() == nullptr) continue;
						if (p->getTimerSeconds() >= ConnectionTimeOut.load(std::memory_order_acquire)) {
							p->CloseSock();
							PSC_Pool.drop_nolock(p.get());
						}
					}
					lockPSC.unlock();
					PSC_Pool.clean();
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
			std::atomic_int32_t PoolCleanLimit, CleanTimeInterval, ConnectionTimeOut;//内存池待清理数量上限，检查客户端连接是否超时间隔，客户端连接最长存活时间
			std::atomic_bool stopFlag;
			std::condition_variable cleanerCV;
			std::mutex cleanerMut;

			WinSock::Address bindAddr;

			IOCPSpace::Pool<PerSocketContext> PSC_Pool;
		};

	}

}

#endif