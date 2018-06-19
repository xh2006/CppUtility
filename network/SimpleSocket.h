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

#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 1024

enum NETWORK_PATTERN
{
	np_server,
	np_client
};

struct SOCK_MESSAGE_HEADER
{
	UINT dataType;
	UINT headerSize;
	UINT dataSize;
};

typedef void(*HandleDataFunc)(SOCK_MESSAGE_HEADER& smh, const char* pDataBuf);

class CSimpleSocket
{
public:
	CSimpleSocket();
	~CSimpleSocket();

	BOOL StartNetwork(std::string strIP, unsigned short uPort, NETWORK_PATTERN np, HandleDataFunc pHandleDataFunc);
	void StopNetwork();

	bool GetNetworkConnStatus();

	BOOL SendData(int dataType, const char* pData, int nDataSize);

private:
	void CreateMsgServerTcp(std::string strIP, unsigned short uPort);
	void CreateMsgClientTcp(std::string strIP, unsigned short uPort);
	void RecvData(SOCKET& connSocket);

	SOCKET m_srvSocket;		// Create a SOCKET for accepting incoming requests.
	SOCKET m_cltSocket;
	NETWORK_PATTERN m_workPattern;	// 0£ºserver ; 1£ºclient
	bool m_bStop;	// network 
	bool m_bConnected;	// 
	HandleDataFunc HandleData;
};

CSimpleSocket::CSimpleSocket()
{
	m_srvSocket = INVALID_SOCKET;
	m_cltSocket = INVALID_SOCKET;
	m_bStop = false;
	m_bConnected = false;

	//----------------------
	// Initialize Winsock.
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		// log ("Error at WSAStartup()\n");
		return;
	}
}

CSimpleSocket::~CSimpleSocket()
{
	closesocket(m_srvSocket);
	closesocket(m_cltSocket);
	WSACleanup();
}

void CSimpleSocket::CreateMsgServerTcp(std::string strIP, unsigned short uPort)
{
	//----------------------
	// Create a SOCKET for listening, for incoming connection requests.
	SOCKET listenSocket;
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET) {
		// log ("Error at socket(): %ld\n", WSAGetLastError());
		return;
	}

	//----------------------
	// The sockaddr_in structure specifies the address family,
	// IP address, and port for the socket that is being bound.
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr(strIP.c_str());
	service.sin_port = htons(uPort);

	if ( bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
		// log ("bind() failed.\n");
		closesocket(listenSocket);
		return;
	}

	//----------------------
	// Listen for incoming connection requests on the created socket
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		// ("Error listening on socket.\n");
		closesocket(listenSocket);
		return;
	}

	while (!m_bStop)
	{
		m_bConnected = false;
		//----------------------
		// Accept the connection.
		m_srvSocket = accept(listenSocket, NULL, NULL);
		if (m_srvSocket == INVALID_SOCKET)
		{
			// log ("accept failed: %d\n", WSAGetLastError());
			closesocket(listenSocket);
		}

		RecvData(m_srvSocket);
	}
}

void CSimpleSocket::CreateMsgClientTcp(std::string strIP, unsigned short uPort)
{
	int iResult = 0;
	struct sockaddr_in serviceAddr;
	//----------------------
	// Create a SOCKET for sending, for incoming connection requests.
	m_cltSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_cltSocket == INVALID_SOCKET) {
		// log ("Error at socket(): %ld\n", WSAGetLastError());
		WSACleanup();
		return;
	}

	//----------------------
	serviceAddr.sin_family = AF_INET;
	serviceAddr.sin_addr.s_addr = inet_addr(strIP.c_str());
	serviceAddr.sin_port = htons(uPort);

	//----------------------
	// Connect to server.
	while (!m_bStop)
	{
		m_bConnected = false;
		do
		{
			iResult = connect(m_cltSocket, (SOCKADDR*)&serviceAddr, sizeof(serviceAddr));
			if (iResult == SOCKET_ERROR)
			{
				// log ("connect failed with error: %d\n", WSAGetLastError());
			}
			Sleep(500);
		} while (iResult);

		RecvData(m_cltSocket);
	}
}

void CSimpleSocket::RecvData(SOCKET& connSocket)
{
	char dataBuf[BUF_SIZE * 2] = {0};
	char* pIncr = dataBuf;
	int nRecvDataSize = 0;
	// socket recv
	char recvBuf[BUF_SIZE];
	char* pBuf = NULL;

	while (true)
	{
		if (connSocket == INVALID_SOCKET)
		{
			return;
		}
		m_bConnected = true;

		int nRecvSize = recv(connSocket, recvBuf, sizeof(recvBuf), 0);
		if (nRecvSize > 0)
		{
			nRecvDataSize += nRecvSize;
			memcpy(pIncr, recvBuf, nRecvSize);
			pBuf = dataBuf;
			int nDataSize = 0;

			while (true)
			{
				nDataSize = ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize + ((SOCK_MESSAGE_HEADER*)pBuf)->dataSize;
				if (nRecvDataSize >= nDataSize)
				{
					HandleData(*((SOCK_MESSAGE_HEADER*)pBuf), pBuf + ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize);
					pBuf = pBuf + nDataSize;
					nRecvDataSize -= nDataSize;
					if (nRecvDataSize == 0)
					{
						break;
					}
					continue;
				}
				else
				{
					if (nDataSize > BUF_SIZE * 2)
					{
						// log
					}
					memcpy(dataBuf, pBuf, nRecvDataSize);
					pIncr = dataBuf + nRecvDataSize;
					break;
				}
			}
		}
		else if (nRecvSize == 0)
		{
			// connection has closed. To do ...
			connSocket = INVALID_SOCKET;
		}
		else
		{
			int nRet = WSAGetLastError();
			// log
			if (WSAEMSGSIZE == nRet)
			{
			}		
			connSocket = INVALID_SOCKET;
		}
	}
}

// To do ... (for nDataSize larger than 1000)
BOOL CSimpleSocket::SendData(int dataType, const char* pData, int nDataSize)
{
	BOOL bRet = FALSE;
	SOCKET comSocket = m_workPattern ? m_cltSocket : m_srvSocket;
	if (comSocket != INVALID_SOCKET)
	{
		char buf[BUF_SIZE];
		SOCK_MESSAGE_HEADER smh;
		smh.dataType = dataType;
		smh.headerSize = sizeof(smh);
		smh.dataSize = nDataSize;
		memcpy(buf, &smh, smh.headerSize);
		memcpy(buf + smh.headerSize, pData, nDataSize);
		int iResult = send(comSocket, buf, smh.headerSize + smh.dataSize, 0);

		if (iResult == SOCKET_ERROR)
		{
			// log
			bRet = FALSE;
		}
	}
	
	return bRet;
}

BOOL CSimpleSocket::StartNetwork(std::string strIP, unsigned short uPort, NETWORK_PATTERN np, HandleDataFunc pHandleDataFunc)
{
	BOOL bRet = TRUE;
	if (strIP.empty() || uPort == 0 || pHandleDataFunc == NULL || m_bConnected)
	{
		return FALSE;
	}
	HandleData = pHandleDataFunc;
	m_workPattern = np;
	if (m_workPattern)
	{
		std::thread thread_clt(&CSimpleSocket::CreateMsgClientTcp, this, strIP, uPort);
		thread_clt.detach();
	}
	else
	{
		std::thread thread_srv(&CSimpleSocket::CreateMsgServerTcp, this, strIP, uPort);
		thread_srv.detach();
	}
	return bRet;
}

void CSimpleSocket::StopNetwork()
{
	m_bStop = true;
	if (m_cltSocket != INVALID_SOCKET)
	{
		closesocket(m_cltSocket);
	}

	if (m_srvSocket != INVALID_SOCKET)
	{
		shutdown(m_srvSocket, SD_SEND);
	}
}

bool CSimpleSocket::GetNetworkConnStatus()
{
	return m_bConnected;
}


#endif