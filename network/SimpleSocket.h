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
#include <mutex>
#include <functional>

#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

namespace SIMPLE_SOCKET
{

#define BUF_SIZE 1024

	enum NETWORK_PATTERN
	{
		np_client,
		np_server,		// which can be connected by only one client, but can be connected many times.
		np_client_ex,
		np_server_ex
	};

	enum FUNC_STYLE
	{
		fs_null,
		fs_normal,
		fs_stdBind
	};

	struct SOCK_MESSAGE_HEADER
	{
		UINT dataType;
		UINT headerSize;
		UINT dataSize;
	};

	struct SOCKET_INFO
	{
		bool bAlive;
	};
	typedef std::map<SOCKET, SOCKET_INFO> SOCKET_INFOS;

	typedef void(*HandleDataFunc)(const SOCKET connSocket, SOCK_MESSAGE_HEADER& smh, const char* pDataBuf);
	typedef std::function<void(const SOCKET connSocket, SOCK_MESSAGE_HEADER& smh, const char* pDataBuf)> STD_HandleDataFunc;

	class CSimpleSocket
	{
	public:
		CSimpleSocket();
		virtual ~CSimpleSocket();

		BOOL StartNetwork(std::string strIP, unsigned short uPort, NETWORK_PATTERN np, HandleDataFunc pHandleDataFunc);
		BOOL StartNetwork(std::string strIP, unsigned short uPort, NETWORK_PATTERN np, STD_HandleDataFunc pHandleDataFunc);
		void StopNetwork();

		bool GetNetworkConnStatus();

		BOOL SendData(int dataType, const char* pData, int nDataSize);
		static BOOL SendData(const SOCKET& s, int dataType, const char* pData, int nDataSize);

	private:
		void CreateMsgServerTcp(std::string strIP, unsigned short uPort);
		void CreateMsgClientTcp(std::string strIP, unsigned short uPort);
		void RecvData(SOCKET& connSocket);
		void RecvDataEx(SOCKET& connSocket);

		void SetSocketFailed(SOCKET& s);

		SOCKET m_cltSocket;
		NETWORK_PATTERN m_workPattern;	// Describe the network work pattern. 0£ºclient ; > 1£ºserver
		bool m_bStop;	// network 
		bool m_bConnected;	// 
		HandleDataFunc HandleData;
		STD_HandleDataFunc STD_HandleData;
		SOCKET_INFOS m_socketInfos;
		std::mutex mutex_socketInfos;
		FUNC_STYLE m_fsCallBack;
	};

	

}

#endif