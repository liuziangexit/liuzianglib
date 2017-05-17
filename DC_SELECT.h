#pragma once
#ifndef liuzianglib_SELECT
#define liuzianglib_SELECT
#include <winsock2.h>
#include "liuzianglib.h"
#include "DC_WinSock.h"
#pragma comment(lib,"ws2_32.lib")
//Version 2.4.2V35
//20170517

namespace DC {

	namespace Web {

		namespace Server {

			namespace SELECT {

				class Server {
				public:
					Server(const std::size_t& inputThreadNumber) :TP(nullptr), ThreadNumber(inputThreadNumber), m_listen(INVALID_SOCKET) {}

					Server(const Server&) = delete;

					virtual ~Server()noexcept = 0;

				public:
					bool Start() {
						if (DC::isNull(TP)) {
							try {
								TP = new DC::ThreadPool(ThreadNumber + 1);
							}
							catch (std::bad_alloc&) {
								OnError(DC::DC_ERROR("Start", "new operator error"));
							}
							catch (...) {}
						}
					}

					void Stop() {}

				protected:
					virtual inline const std::size_t getRecvBufferSize()noexcept {//recv »º³åÇø
						return 1024;
					}

					virtual void DoRecv() {}

					virtual void OnError(const DC::DC_ERROR&) {}

				private:
					bool ListenerThread()noexcept {
						try {
							WinSock::SocketInit_TCP(m_listen);
							if (m_listen == SOCKET_ERROR) return false;

							if (!WinSock::Bind(m_listen, bindAddr)) { closesocket(m_listen); return false; }
							if (!WinSock::Listen(m_listen, SOMAXCONN)) { closesocket(m_listen); return false; }

							SOCKET acceptSocket;
							DC::WinSock::Address ClientAddr;

							while (true) {
								acceptSocket = INVALID_SOCKET;
								if (!DC::WinSock::Accept(acceptSocket, m_listen, ClientAddr)) { closesocket(m_listen); return false; }

								
							}

							return true;
						}
						catch (DC::DC_ERROR& err) { OnError(err); return false; }
						catch (...) { OnError(DC::DC_ERROR("ListenerThread", "unknown error")); return false; }
						return false;
					}

				private:					
					ThreadPool *TP;
					const std::size_t ThreadNumber;

					SOCKET m_listen;
					WinSock::Address bindAddr;
				};

			}

		}

	}

}

#endif