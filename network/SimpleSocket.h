/*!
 * group: GroupName
 * author: Xu Hua
 * date: 2018
 * Contact: xh2010wzx@163.com
 * class: CSimpleSocket
 * version 1.0
 * brief: 
 *
 * TODO: long description
 *
 * note: 
 * 
 * Example:
 * --------------------------------For windows:------------------------------------
 // The callback function, define by user.
 void HandleData(SOCK_MESSAGE_HEADER& smh, const char* pDataBuf)
 {
	 std::string strData;
	 CS_MSG* pMsg = NULL;
	 switch (smh.dataType)
	 {
	 case DT_STRING:	
		 strData = std::string(pDataBuf, pDataBuf + smh.dataSize);
		 printf(strData.c_str());
		 break;
	 case DT_MSG:
		 pMsg = (CS_MSG*)pDataBuf;
		 printf("msgId : %d, dwHandle : %d", pMsg->msgId, pMsg->dwHandle);
		 break;
	 default:
		 break;
	 }
 }

For Server:
	 CSimpleSocket svr;
	 svr.StartNetwork("127.0.0.1", 8005, np_server, HandleData);
	 
	 while (!svr.GetNetworkConnStatus())
	 {
		 Sleep(500);
	 }
	 svr.SendData(DT_STRING, "From server \n", sizeof("From server \n"));

For Client:
	 CSimpleSocket clt;
	 clt.StartNetwork("127.0.0.1", 8005, np_client, HandleData);

	 while (!clt.GetNetworkConnStatus())
	 {
		 Sleep(500);
	 }
	 clt.SendData(DT_STRING, "From client \n", sizeof("From client \n"));

 *
 */

#pragma once
#ifndef _SIMPLE_SOCKET_H_
#define _SIMPLE_SOCKET_H_

#include <string>
#include <thread>
#include <map>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <memory>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#endif

namespace SIMPLE_SOCKET
{
// socket representative
#ifdef _WIN32
#define socket_r		intptr_t
#else
#define socket_r 		int
#define INVALID_SOCKET 	(socket_r)(~0)
#define SOCKET_ERROR 	-1
#define closesocket		close
#define MAX_EVENTS		10
#endif

#define BUF_SIZE		1024
#define LOG_BUF_SIZE	256

#define SAFE_COPY(DST, SRC, COUNT)	DST + COUNT > SRC ? memmove(DST, SRC, COUNT) : memcpy(DST, SRC, COUNT)


	enum NETWORK_PATTERN
	{
#ifdef _WIN32
		np_client,
		np_server,		// for data whose largest size is less than BUF_SIZE.
		np_client_ex,	
		np_server_ex	// for big data with no fixed size.
#else	// For linux, we only support epoll now. 
		np_client,
		np_server_epoll_lt,
		np_server_epoll_et
#endif
	};

	enum FUNC_STYLE
	{
		fs_null,
		fs_normal,
		fs_stdBind
	};

	struct SOCK_MESSAGE_HEADER
	{
		uint16_t headerSize;
		uint16_t dataType;
		uint32_t dataSize;
	};

	struct SOCKET_INFO
	{
		bool bAlive;
		sockaddr clientIP;
		ushort clientPort;
		char* pBuf;		// point to a buf which save the socket's data.
		// std::share_ptr<char*> pBuf;
		int nRecvSize;
	};
	typedef std::unordered_map<socket_r, SOCKET_INFO> SOCKET_INFOS;

	typedef void(*HandleDataFunc)(const socket_r connSocket, SOCK_MESSAGE_HEADER& smh, const char* pDataBuf);
	typedef std::function<void(const socket_r connSocket, SOCK_MESSAGE_HEADER& smh, const char* pDataBuf)> STD_HandleDataFunc;
	typedef void(*LogFunc)(const char* pLogStr);
	typedef void(*LogFuncW)(const wchar_t* pLogStr);
	typedef std::function<void(const std::wstring& strLog)> STD_LogFuncW;
	typedef std::function<void(const std::string& strLog)> STD_LogFunc;

	class CSimpleSocket
	{
	public:
		CSimpleSocket();
		virtual ~CSimpleSocket();

		bool StartNetwork(const std::string strIP, unsigned short uPort, NETWORK_PATTERN np, HandleDataFunc pHandleDataFunc);
		bool StartNetwork(const std::string strIP, unsigned short uPort, NETWORK_PATTERN np, STD_HandleDataFunc pHandleDataFunc);
		void StopNetwork();
		void SetLogCallBackFunc(LogFunc logFunc);
		void SetLogCallBackFunc(STD_LogFunc logFunc);
		bool GetNetworkConnStatus();

		bool SendData(int dataType, const char* pData, int nDataSize);
		static bool SendData(const socket_r& s, int dataType, const char* pData, int nDataSize);

	private:
		void CreateMsgServerTcp(const std::string strIP, unsigned short uPort);
		void CreateMsgClientTcp(const std::string strIP, unsigned short uPort);
		void RecvData(socket_r& connSocket);	// for package size less than BUF_SIZE
		void RecvDataEx(socket_r& connSocket);	// for package size is not fixed.

		int GetSocketErrorCode();
		void SetSocketFailed(socket_r& s);
		inline void LogOutput(const char* pLogStr);
		inline void Log(const char* FormatStr, ...);
		inline void AddSocketInfo(const socket_r& connSock);
		inline void DelSocketInfo(const socket_r& connSock);

#ifndef _WIN32
		int set_nonblocking(int fd);
		void add_fd(int epollfd, int fd, bool enable_et = true);
		void remove_fd(int epollfd, int fd);
		inline void lt(epoll_event* events, int num, int epollfd, int listenfd);
		inline void et(epoll_event* events, int num, int epollfd, int listenfd);
		// would have a blend pattern?
		inline void handle_epoll_event(epoll_event* events, int num, int epollfd, int listenfd);

		int m_epfd;
#endif

		HandleDataFunc HandleData;
		STD_HandleDataFunc STD_HandleData;
		LogFunc CallBackLog;
		STD_LogFunc STD_CallBackLog;

		socket_r m_cltSocket;
		NETWORK_PATTERN m_workPattern;	// Describe the network work pattern. 0��client ; > 1��server
		bool m_bStop;	// network 
		bool m_bConnected;	// 

		SOCKET_INFOS m_socketInfos;
		std::mutex m_mutexSocketInfos;
		FUNC_STYLE m_fsCallBack;	
	};
}

#endif