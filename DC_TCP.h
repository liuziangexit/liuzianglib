#pragma once
#ifndef liuzianglib_TCP
#define liuzianglib_TCP
#include <winsock2.h>
#include <string>
#include <memory>
#pragma comment(lib, "ws2_32.lib")
//Version 2.4.1V10
//20170323
//注意:DC_TCP.h必须先于DC_MySQL.h包含

namespace DC {

	namespace TCP {

		sockaddr_in MakeAddr(const std::string& address, std::size_t port) {
			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			addr.sin_addr.S_un.S_addr = inet_addr(address.c_str());
			return addr;
		}

		inline bool Startup() {
			WORD SockVersion = MAKEWORD(2, 2);
			WSADATA WsaData;
			if (WSAStartup(SockVersion, &WsaData) != 0) {
				return false;
			}
			return true;
		}

		inline void Cleanup() {
			WSACleanup();
		}

		inline void SocketInit(SOCKET& s) {
			s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}

		inline bool Bind(SOCKET s, sockaddr_in addr) {
			if (bind(s, (LPSOCKADDR)&addr, sizeof(addr)) != 0) {
				return false;
			}
			return true;
		}

		inline void UnBind(SOCKET s) {
			closesocket(s);
		}

		bool Listen(SOCKET s, int QueueSize) {
			if (listen(s, QueueSize) != 0) {
				return false;
			}
			return true;
		}

		inline void UnListen(SOCKET s) {
			closesocket(s);
		}

		bool GetConnection(SOCKET& Sock, SOCKET SockListen, sockaddr_in& RemoteAddr) {
			int RemoteAddrLen = sizeof(RemoteAddr);
			Sock = accept(SockListen, (sockaddr*)&RemoteAddr, &RemoteAddrLen);
			if (Sock == INVALID_SOCKET) {
				return false;
			}
			return true;
		}

		inline bool Connect(SOCKET s, sockaddr_in addr) {
			if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
				return false;
			}
			return true;
		}

		bool Send(SOCKET s, const std::string& str) {
			if (send(s, str.c_str(), str.size(), 0) != SOCKET_ERROR) return true;
			return false;
		}

		bool Recv(SOCKET s, std::string& str, std::size_t len) {
			bool returnvalue = false;
			std::unique_ptr<char[]> recvMessagech(new char[len + 1]);
			int ret = recv(s, recvMessagech.get(), len, 0);
			if (ret > 0) {
				recvMessagech.get()[ret] = NULL;
				str = recvMessagech.get();
				returnvalue = true;
			}
			return returnvalue;
		}

		void Close(SOCKET s) {
			closesocket(s);
		}

		bool SetRecvTimeOut(SOCKET s, std::size_t limit) {
			if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&limit, sizeof(int)) == SOCKET_ERROR) return false;
			return true;
		}

		bool SetSendTimeOut(SOCKET s, std::size_t limit) {
			if (setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&limit, sizeof(int)) == SOCKET_ERROR) return false;
			return true;
		}

	}

}

#endif