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
// #include <windows.h>
#pragma comment(lib, "Ws2_32.lib")

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

CSimpleSocket::CSimpleSocket()
{
	m_bStop = false;
	m_bConnected = false;
	m_fsCallBack = fs_null;
	m_cltSocket = INVALID_SOCKET;
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
	if (m_cltSocket != INVALID_SOCKET)
	{
		closesocket(m_cltSocket);
	}
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
		SOCKET connSock;
		//----------------------  Accept the connection.
		connSock = accept(listenSocket, NULL, NULL);
		if (connSock == INVALID_SOCKET)
		{
			// log ("accept failed: %d\n", WSAGetLastError());
			closesocket(listenSocket);
		}

		if (m_workPattern == np_server)
		{
			SOCKET_INFO si;
			si.bAlive = true;
			m_socketInfos[connSock] = si;
			std::thread th(&CSimpleSocket::RecvData, this, connSock);
			th.detach();
		}
		else if (m_workPattern == np_server_ex)
		{
			SOCKET_INFO si;
			si.bAlive = true;
			m_socketInfos[connSock] = si;
			std::thread th(&CSimpleSocket::RecvDataEx, this, connSock);
			th.detach();
		}
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

		if (m_workPattern == np_client)
		{
			RecvData(m_cltSocket);
		}
		else if (m_workPattern == np_client_ex)
		{
			RecvDataEx(m_cltSocket);
		}
	}
}

void CSimpleSocket::RecvData(SOCKET& connSocket)
{
	SOCKET s = connSocket;
	char dataBuf[BUF_SIZE * 2] = {0};
	char* pIncr = dataBuf;
	int nRecvDataSize = 0;
	// socket recv
	char recvBuf[BUF_SIZE];
	char* pBuf = NULL;

	while (true)
	{
		if (s == INVALID_SOCKET)
		{
			break;
		}
		m_bConnected = true;

		int nRecvSize = recv(s, recvBuf, sizeof(recvBuf), 0);
		if (nRecvSize > 0)
		{
			nRecvDataSize += nRecvSize;
			memcpy(pIncr, recvBuf, nRecvSize);
			pBuf = dataBuf;
			pIncr += nRecvSize;
			int nDataSize = 0;

			while (true)
			{
				nDataSize = ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize + ((SOCK_MESSAGE_HEADER*)pBuf)->dataSize;
				if (nRecvDataSize >= nDataSize)
				{
					if (m_fsCallBack == fs_normal)
					{
						HandleData(s, *((SOCK_MESSAGE_HEADER*)pBuf), pBuf + ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize);
					}
					else if (m_fsCallBack == fs_stdBind)
					{
						STD_HandleData(s, *((SOCK_MESSAGE_HEADER*)pBuf), pBuf + ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize);
					}

					pBuf = pBuf + nDataSize;
					nRecvDataSize -= nDataSize;
					if (nRecvDataSize == 0)
					{
						pIncr = dataBuf;
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
			s = INVALID_SOCKET;
		}
		else
		{
			int nRet = WSAGetLastError();
			// log
			if (WSAEMSGSIZE == nRet)
			{
			}		
			s = INVALID_SOCKET;
		}
	}
}

void CSimpleSocket::RecvDataEx(SOCKET& connSocket)
{
	int nDataBufSize = BUF_SIZE * 2;
	char* pDataBuf = new char[nDataBufSize];
	char* pIncr = pDataBuf;
	int nRecvDataSize = 0;
	char recvBuf[BUF_SIZE];		// socket recv buf
	char* pBuf = pDataBuf;
	int nDataSize = 0;

	while (true)
	{
		if (connSocket == INVALID_SOCKET)
		{
			break;
		}
		m_bConnected = true;
		
		int nRecvSize = recv(connSocket, recvBuf, sizeof(recvBuf), 0);
		if (nRecvSize > 0)
		{
			if ((nDataBufSize - (pIncr - pDataBuf)) < nRecvSize)
			{
				memcpy(pDataBuf, pBuf, nRecvDataSize);
				pIncr = pDataBuf + nRecvDataSize;
				pBuf = pDataBuf;
			}
			// copy data to data buf.
			memcpy(pIncr, recvBuf, nRecvSize);
			nRecvDataSize += nRecvSize;
			pIncr += nRecvSize;

			while (true)
			{
				if (!nDataSize)
				{
					nDataSize = ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize + ((SOCK_MESSAGE_HEADER*)pBuf)->dataSize;
				}
				if (nDataSize > nDataBufSize)
				{
					nDataBufSize = nDataSize % BUF_SIZE ? (nDataSize / BUF_SIZE + 2) * BUF_SIZE : (nDataSize / BUF_SIZE + 1) * BUF_SIZE;
					char* pNewDataBuf = new char[nDataBufSize];
					if (pNewDataBuf == NULL)
					{
						// log
						return;
					}
					// copy data to new data buf.
					memcpy(pNewDataBuf, pBuf, nRecvDataSize);
					// release original buf
					if (pDataBuf)
						delete []pDataBuf;
					
					pDataBuf = pNewDataBuf;		
					pBuf = pIncr = pDataBuf;
					pIncr += nRecvDataSize;
					break;
				}

				if (nRecvDataSize >= nDataSize)
				{
					if (m_fsCallBack == fs_normal)
					{
						HandleData(connSocket, *((SOCK_MESSAGE_HEADER*)pBuf), pBuf + ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize);
					}
					else if (m_fsCallBack == fs_stdBind)
					{
						STD_HandleData(connSocket, *((SOCK_MESSAGE_HEADER*)pBuf), pBuf + ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize);
					}
					pBuf = pBuf + nDataSize;
					nRecvDataSize -= nDataSize;
					nDataSize = 0;
					if (nRecvDataSize < sizeof(SOCK_MESSAGE_HEADER))
					{
						memcpy(pDataBuf, pBuf, nRecvDataSize);
						pBuf = pDataBuf;
						pIncr = pDataBuf + nRecvDataSize;
						break;
					}
					continue;
				}
				else
				{
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

	if (pDataBuf)
		delete []pDataBuf;
}

BOOL CSimpleSocket::SendData(const SOCKET& s, int dataType, const char* pData, int nDataSize)
{
	BOOL bRet = FALSE;
	char* pAllocBuf = NULL;
	char* pBuf = NULL;
	if (s != INVALID_SOCKET)
	{
		char buf[BUF_SIZE];
		if (nDataSize > sizeof(buf))
		{
			pAllocBuf = new char[sizeof(SOCK_MESSAGE_HEADER) + nDataSize];
			if (pAllocBuf)
			{
				pBuf = pAllocBuf;
			}
			else
			{
				// log
			}
		}
		else
		{
			pBuf = buf;
		}
		SOCK_MESSAGE_HEADER smh;
		smh.dataType = dataType;
		smh.headerSize = sizeof(smh);
		smh.dataSize = nDataSize;
		memcpy(pBuf, &smh, smh.headerSize);
		memcpy(pBuf + smh.headerSize, pData, nDataSize);
		int iResult = send(s, pBuf, smh.headerSize + smh.dataSize, 0);

		bRet = TRUE;
		if (iResult == SOCKET_ERROR)
		{
			// log
			bRet = FALSE;
		}
	}

	if (pAllocBuf)
		delete[]pAllocBuf;

	return bRet;
}

BOOL CSimpleSocket::SendData(int dataType, const char* pData, int nDataSize)
{
	return SendData(m_cltSocket, dataType, pData, nDataSize);
}

BOOL CSimpleSocket::StartNetwork(std::string strIP, unsigned short uPort, NETWORK_PATTERN np, HandleDataFunc pHandleDataFunc)
{
	BOOL bRet = TRUE;
	if (strIP.empty() || uPort == 0 || pHandleDataFunc == NULL || m_bConnected)
	{
		return FALSE;
	}
	m_fsCallBack = fs_normal;
	HandleData = pHandleDataFunc;
	m_workPattern = np;

	if (m_workPattern == np_client)
	{
		std::thread thread_clt(&CSimpleSocket::CreateMsgClientTcp, this, strIP, uPort);
		thread_clt.detach();
	}
	else if (m_workPattern == np_server)
	{
		std::thread thread_srv(&CSimpleSocket::CreateMsgServerTcp, this, strIP, uPort);
		thread_srv.detach();
	}

	return bRet;
}

BOOL CSimpleSocket::StartNetwork(std::string strIP, unsigned short uPort, NETWORK_PATTERN np, STD_HandleDataFunc pHandleDataFunc)
{
	BOOL bRet = TRUE;
	if (strIP.empty() || uPort == 0 || pHandleDataFunc == NULL || m_bConnected)
	{
		return FALSE;
	}
	m_fsCallBack = fs_stdBind;
	STD_HandleData = pHandleDataFunc;
	m_workPattern = np;

	if (m_workPattern == np_client)
	{
		std::thread thread_clt(&CSimpleSocket::CreateMsgClientTcp, this, strIP, uPort);
		thread_clt.detach();
	}
	else if (m_workPattern == np_server)
	{
		std::thread thread_srv(&CSimpleSocket::CreateMsgServerTcp, this, strIP, uPort);
		thread_srv.detach();
	}

	return bRet;
}

void CSimpleSocket::StopNetwork()
{
	m_bStop = true;
	std::lock_guard<std::mutex> mlock(mutex_socketInfos);

	switch (m_workPattern)
	{
	case np_client:
		closesocket(m_cltSocket);
		break;
	case np_server:
		for (auto it = m_socketInfos.begin(); it != m_socketInfos.end(); it++)
		{
			shutdown(it->first, SD_SEND);
		}
		m_socketInfos.clear();
		break;
	default:
		break;
	}
}

bool CSimpleSocket::GetNetworkConnStatus()
{
	return m_bConnected;
}

void CSimpleSocket::SetSocketFailed(SOCKET& s)
{
	std::lock_guard<std::mutex> mlock(mutex_socketInfos);
	auto iter = m_socketInfos.find(s);
	if (iter != m_socketInfos.end())
	{
		// (*iter).second.bAlive = false;
		// For now, just erase the socket record.
		m_socketInfos.erase(iter);
	}
}


#endif