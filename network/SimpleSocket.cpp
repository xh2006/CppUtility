
#include "SimpleSocket.h"

using namespace SIMPLE_SOCKET;

CSimpleSocket::CSimpleSocket()
{
	m_bStop = false;
	m_bConnected = false;
	m_fsCallBack = fs_null;
	m_cltSocket = INVALID_SOCKET;
	HandleData = NULL;
	STD_HandleData = nullptr;
	CallBackLog = NULL;
	STD_CallBackLog = nullptr;
	//----------------------
	// Initialize Winsock.
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		Log(L"Error at WSAStartup()\n");
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
		Log(L"Error at socket(): %ld\n", WSAGetLastError());
		return;
	}

	//----------------------
	// The sockaddr_in structure specifies the address family,
	// IP address, and port for the socket that is being bound.
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr(strIP.c_str());
	service.sin_port = htons(uPort);

	if (::bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
		Log(L"bind() failed. Error code : %d\n", WSAGetLastError());
		closesocket(listenSocket);
		return;
	}

	//----------------------
	// Listen for incoming connection requests on the created socket
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		Log(L"Error listening on socket. Error code : %d\n", WSAGetLastError());
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
			Log(L"accept failed: %d\n", WSAGetLastError());
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
	while (!m_bStop)
	{	
		//----------------------
		// Create a SOCKET for sending, for incoming connection requests.
		m_cltSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_cltSocket == INVALID_SOCKET) {
			Log (L"Error at socket(): %ld\n", WSAGetLastError());
			WSACleanup();
			return;
		}

		//----------------------
		serviceAddr.sin_family = AF_INET;
		serviceAddr.sin_addr.s_addr = inet_addr(strIP.c_str());
		serviceAddr.sin_port = htons(uPort);

		//----------------------
		// Connect to server.
		m_bConnected = false;
		do
		{
			Log(L"try to connect to server...... \n");
			iResult = connect(m_cltSocket, (SOCKADDR*)&serviceAddr, sizeof(serviceAddr));
			if (iResult == SOCKET_ERROR)
			{
				Log(L"connect failed with error: %d \n", WSAGetLastError());
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
	char dataBuf[BUF_SIZE * 2] = { 0 };
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
					if (m_fsCallBack == fs_normal && HandleData != NULL)
					{
						HandleData(s, *((SOCK_MESSAGE_HEADER*)pBuf), pBuf + ((SOCK_MESSAGE_HEADER*)pBuf)->headerSize);
					}
					else if (m_fsCallBack == fs_stdBind && STD_HandleData != nullptr)
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
						Log(L"RecvData, Send data size larger than buf, the data size is %d. \n", nDataSize);
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
			Log(L"RecvData Error : %d \n", WSAGetLastError());
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
	char* pData = pDataBuf;
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
				if (nRecvDataSize + nRecvSize > nDataBufSize)
				{
					int nSaveSize = nRecvDataSize + nRecvSize;
					nDataBufSize = nSaveSize % BUF_SIZE ? (nSaveSize / BUF_SIZE + 2) * BUF_SIZE : (nSaveSize / BUF_SIZE + 1) * BUF_SIZE;
					char* pNewDataBuf = new char[nDataBufSize];
					if (pNewDataBuf == NULL)
					{
						Log(L"RecvDataEx: allocate new space failed, the need size is %d \n", nDataBufSize);
						return;
					}
					// copy data to new data buf.
					memcpy(pNewDataBuf, pData, nRecvDataSize);
					// release original buf
					if (pDataBuf)
						delete[]pDataBuf;

					pDataBuf = pNewDataBuf;
					pData = pIncr = pDataBuf;
					pIncr += nRecvDataSize;
				}
				else
				{
					memcpy(pDataBuf, pData, nRecvDataSize);
					pIncr = pDataBuf + nRecvDataSize;
					pData = pDataBuf;
				}
			}
			// copy data to data buf.
			memcpy(pIncr, recvBuf, nRecvSize);
			nRecvDataSize += nRecvSize;
			pIncr += nRecvSize;

			while (true)
			{
				if (!nDataSize)
				{
					nDataSize = ((SOCK_MESSAGE_HEADER*)pData)->headerSize + ((SOCK_MESSAGE_HEADER*)pData)->dataSize;
				}
				if (nDataSize > nDataBufSize)
				{
					nDataBufSize = nDataSize % BUF_SIZE ? (nDataSize / BUF_SIZE + 2) * BUF_SIZE : (nDataSize / BUF_SIZE + 1) * BUF_SIZE;
					char* pNewDataBuf = new char[nDataBufSize];
					if (pNewDataBuf == NULL)
					{
						Log(L"RecvDataEx: allocate new space failed, the need size is %d \n", nDataBufSize);
						return;
					}
					// copy data to new data buf.
					memcpy(pNewDataBuf, pData, nRecvDataSize);
					// release original buf
					if (pDataBuf)
						delete[]pDataBuf;

					pDataBuf = pNewDataBuf;
					pData = pIncr = pDataBuf;
					pIncr += nRecvDataSize;
					break;
				}

				if (nRecvDataSize >= nDataSize)
				{
					if (m_fsCallBack == fs_normal && HandleData != NULL)
					{
						HandleData(connSocket, *((SOCK_MESSAGE_HEADER*)pData), pData + ((SOCK_MESSAGE_HEADER*)pData)->headerSize);
					}
					else if (m_fsCallBack == fs_stdBind && STD_HandleData != nullptr)
					{
						STD_HandleData(connSocket, *((SOCK_MESSAGE_HEADER*)pData), pData + ((SOCK_MESSAGE_HEADER*)pData)->headerSize);
					}
					pData = pData + nDataSize;
					nRecvDataSize -= nDataSize;
					nDataSize = 0;
					if (nRecvDataSize < sizeof(SOCK_MESSAGE_HEADER))
					{
						memcpy(pDataBuf, pData, nRecvDataSize);
						pData = pDataBuf;
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
		{	// connection has closed. To do ...
			connSocket = INVALID_SOCKET;
		}
		else
		{
			Log(L"RecvDataEx Error : %d \n", WSAGetLastError());
			connSocket = INVALID_SOCKET;
		}
	}

	if (pDataBuf)
		delete[]pDataBuf;
}

BOOL CSimpleSocket::SendData(const SOCKET& s, int dataType, const char* pData, int nDataSize)
{
	BOOL bRet = FALSE;
	char* pAllocBuf = NULL;
	char* pBuf = NULL;
	if (s != INVALID_SOCKET)
	{
		char buf[BUF_SIZE];
		if (nDataSize + sizeof(SOCK_MESSAGE_HEADER) > sizeof(buf))
		{
			pAllocBuf = new char[sizeof(SOCK_MESSAGE_HEADER)+nDataSize];
			if (pAllocBuf)
			{
				pBuf = pAllocBuf;
			}
			else
			{
				// Log(L"");
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
	Log(L"Start network ... \n");
	m_fsCallBack = fs_normal;
	HandleData = pHandleDataFunc;
	m_workPattern = np;

	if (m_workPattern == np_client || m_workPattern == np_client_ex)
	{
		std::thread thread_clt(&CSimpleSocket::CreateMsgClientTcp, this, strIP, uPort);
		thread_clt.detach();
	}
	else if (m_workPattern == np_server || m_workPattern == np_server_ex)
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
	Log(L"Start network ... \n");
	m_fsCallBack = fs_stdBind;
	STD_HandleData = pHandleDataFunc;
	m_workPattern = np;

	if (m_workPattern == np_client || m_workPattern == np_client_ex)
	{
		std::thread thread_clt(&CSimpleSocket::CreateMsgClientTcp, this, strIP, uPort);
		thread_clt.detach();
	}
	else if (m_workPattern == np_server || m_workPattern == np_server_ex)
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
			closesocket(it->first);
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

void CSimpleSocket::SetLogCallBackFunc(LogFuncW logFunc)
{
	if (logFunc)
	{
		CallBackLog = logFunc;
		Log(L"Set callback function success. \n");
	}
}

void CSimpleSocket::SetLogCallBackFunc(STD_LogFuncW logFunc)
{
	if (logFunc)
	{
		STD_CallBackLog = logFunc;
		Log(L"Set std callback function success. \n");
	}
}

void CSimpleSocket::LogOutput(const wchar_t* pLogStr)
{
	if (CallBackLog != NULL)
	{
		CallBackLog(pLogStr);
	}
	else if (STD_CallBackLog != nullptr)
	{
		std::wstring strLog = pLogStr;
		STD_CallBackLog(strLog);
	}
	else
	{
		OutputDebugString(pLogStr);
	}
}

void CSimpleSocket::Log(wchar_t* FormatStr, ...)
{
	wchar_t wszLogStr[LOG_BUF_SIZE];
	va_list _Arglist;
	int _Ret;
	_crt_va_start(_Arglist, FormatStr);
	_Ret = _vswprintf_c_l(wszLogStr, sizeof(wszLogStr), FormatStr, NULL, _Arglist);
	_crt_va_end(_Arglist);
	
	if (_Ret > 0)
	{
		LogOutput(wszLogStr);
	}
	else
	{
		LogOutput(L"Log: log error. \n");
	}
}

