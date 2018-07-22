
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include "SimpleSocket.h"

namespace SIMPLE_SOCKET{

CSimpleSocket::CSimpleSocket()
{
	m_bStop = false;
	m_bConnected = false;
	m_fsCallBack = fs_null;
	m_cltSocket = INVALID_SOCKET;
	HandleData = NULL;
	// STD_HandleData = nullptr;
	CallBackLog = NULL;
	// STD_CallBackLog = nullptr;
	//----------------------
#ifdef _WIN32
	// Initialize Winsock.
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		Log("Error at WSAStartup()\n");
		return;
	}
#else
	m_epfd = 0;
#endif
}

CSimpleSocket::~CSimpleSocket()
{
	if (m_cltSocket != INVALID_SOCKET)
	{
		closesocket(m_cltSocket);
	}
#ifdef _WIN32
	WSACleanup();
#else
	if (m_epfd) {
		close(m_epfd);
	}
#endif
	StopNetwork();
}

void CSimpleSocket::CreateMsgServerTcp(std::string strIP, unsigned short uPort)
{
#ifndef _WIN32
	struct epoll_event ev,events[MAX_EVENTS];
	m_epfd = epoll_create(1024);
#endif

	//----------------------
	// Create a socket_r for listening, for incoming connection requests.
	socket_r listenSocket;
	listenSocket = socket(AF_INET, SOCK_STREAM, 0); // IPPROTO_TCP
	if (listenSocket == INVALID_SOCKET) {
		Log("Error at socket(): %d\n", GetSocketErrorCode());
		return;
	}
#ifndef _WIN32
	// add listen socket.
	add_fd(m_epfd, listenSocket);
#endif
	//----------------------
	// The sockaddr_in structure specifies the address family,
	// IP address, and port for the socket that is being bound.
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr(strIP.c_str());
	service.sin_port = htons(uPort);

	if (::bind(listenSocket, (sockaddr*)&service, sizeof(service)) == SOCKET_ERROR) {
		Log("bind() failed. Error code : %d\n", GetSocketErrorCode());
		closesocket(listenSocket);
		return;
	}

	//----------------------
	// Listen for incoming connection requests on the created socket
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		Log("Error listening on socket. Error code : %d\n", GetSocketErrorCode());
		closesocket(listenSocket);
		return;
	}
	m_bConnected = false;
	socket_r connSock;

	while (!m_bStop)
	{
#ifdef _WIN32	// it will be upgraded later if any requirements in pactice. 
		//----------------------  Accept the connection.
		connSock = accept(listenSocket, NULL, NULL);
		if (connSock == INVALID_SOCKET)
		{
			Log("accept failed: %d\n", GetSocketErrorCode());
			closesocket(listenSocket);
		}

		if (m_workPattern == np_server)
		{
			AddSocketInfo(connSock);
			std::thread th(&CSimpleSocket::RecvData, this, connSock);
			th.join();
		}
		else if (m_workPattern == np_server_ex)
		{
			AddSocketInfo(connSock);
			std::thread th(&CSimpleSocket::RecvDataEx, this, connSock);
			th.join();
		}
#else
		int nfds = epoll_wait(m_epfd, events, MAX_EVENTS, 500);

		if(m_workPattern == np_server_epoll_et){
			et(events, nfds, m_epfd, listenSocket);
		}
		else if(m_workPattern == np_server_epoll_lt){
			lt(events, nfds, m_epfd, listenSocket);
		}
#endif
	}
}

void CSimpleSocket::CreateMsgClientTcp(std::string strIP, unsigned short uPort)
{
	int iResult = 0;
	struct sockaddr_in serviceAddr;
	while (!m_bStop)
	{	
		//----------------------
		// Create a socket_r for sending, for incoming connection requests.
		m_cltSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_cltSocket == INVALID_SOCKET) {
			Log ("Error at socket(): %ld\n", GetSocketErrorCode());
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
			Log("try to connect to server...... \n");
			iResult = connect(m_cltSocket, (sockaddr*)&serviceAddr, sizeof(serviceAddr));
			if (iResult == SOCKET_ERROR)
			{
				Log("connect failed with error: %d \n", GetSocketErrorCode());
			}
			// Sleep(500);
		} while (iResult);

		if (m_workPattern == np_client)
		{
			RecvData(m_cltSocket);
		}
#ifdef _WIN32
		else if (m_workPattern == np_client_ex)
		{
			RecvDataEx(m_cltSocket);
		}
#endif
	}
}

void CSimpleSocket::RecvData(socket_r& connSocket)
{
	socket_r s = connSocket;
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
			SAFE_COPY(pIncr, recvBuf, nRecvSize);
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
						Log("RecvData, Send data size larger than buf, the data size is %d. \n", nDataSize);
					}
					SAFE_COPY(dataBuf, pBuf, nRecvDataSize);
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
			Log("RecvData Error : %d \n", GetSocketErrorCode());
			s = INVALID_SOCKET;
		}
	}
}

bool CSimpleSocket::ReallocBufAndCopyData(int nNeedSize, int& nAllocSize, char** ppBuf, char* pData, int nRecvDataSize)
{
    nAllocSize = nNeedSize % BUF_SIZE ? (nNeedSize / BUF_SIZE + 2) * BUF_SIZE : (nNeedSize / BUF_SIZE + 1) * BUF_SIZE;
    char* pNewDataBuf = new char[nAllocSize];
    if (pNewDataBuf == NULL)
    {
        Log("RecvDataEx: allocate new space failed, the need size is %d \n", nAllocSize);
        return false;
    }
    // copy data to new data buf.
    SAFE_COPY(pNewDataBuf, pData, nRecvDataSize);
    // release original buf
    if (*ppBuf)
        delete[](*ppBuf);

    *ppBuf = pNewDataBuf;
    return true;
}

void CSimpleSocket::RecvDataEx(socket_r& connSocket)
{
	int nDataBufSize = BUF_SIZE * 2;	// save the buffer size, the initial size is BUF_SIZE * 2.
	char* pRawDataBuf = new char[nDataBufSize];	// always point to raw data buffer we allocated.
	char* pIncr = pRawDataBuf;		// the increment position in the data buffer.
	int nRecvDataSize = 0;		// the size of received total data in raw data buffer.
	char recvBuf[BUF_SIZE];		// socket recv buf
	char* pPackageData = pRawDataBuf;		// point to header of a data package.
	int nPackageDataSize = 0;

	while (true)
	{
		if (connSocket == INVALID_SOCKET)
		{// if the socket is invalid, we quit the thread.
			break;
		}

		int nRecvSize = recv(connSocket, recvBuf, sizeof(recvBuf), 0);
		if (nRecvSize > 0)
		{	// check if the pRawDataBuf has enough space to save the new arrived data.
            if (nRecvDataSize < sizeof(SOCK_MESSAGE_HEADER))
            {   // when we haven't get a whole header, we just save the received data.
                SAFE_COPY(pIncr, recvBuf, nRecvSize);
                nRecvDataSize += nRecvSize;
                pIncr += nRecvSize;
                continue;
            }

			if ((nDataBufSize - (pIncr - pRawDataBuf)) < nRecvSize)
			{	// check if the raw data buffer can save the new arrive data.
				if (nRecvDataSize + nRecvSize > nDataBufSize)
				{	// if not, realloc the raw data buffer with bigger size.
					int nSaveSize = nRecvDataSize + nRecvSize;
                    if (!ReallocBufAndCopyData(nSaveSize, nDataBufSize, &pRawDataBuf, pPackageData, nRecvDataSize))
                        goto clean;
					pPackageData = pIncr = pRawDataBuf;
					pIncr += nRecvDataSize;
				}
				else
				{	// buffer has been overflow by new data, we rewrite the unhandle data to header of the raw data buffer.
					SAFE_COPY(pRawDataBuf, pPackageData, nRecvDataSize);
					pIncr = pRawDataBuf + nRecvDataSize;
					pPackageData = pRawDataBuf;
				}
			}
			// copy new arrived data to data buffer.
			SAFE_COPY(pIncr, recvBuf, nRecvSize);
			nRecvDataSize += nRecvSize;
			pIncr += nRecvSize;

			while (true)
			{	// handle the received data by calling the callback function.
				if (!nPackageDataSize)
				{
					nPackageDataSize = ((SOCK_MESSAGE_HEADER*)pPackageData)->headerSize + ((SOCK_MESSAGE_HEADER*)pPackageData)->dataSize;
				}
				if (nPackageDataSize > nDataBufSize)
				{	// if the data has not load finished as small size buf, we realloc buffer, and continue to receive data.
                    if (!ReallocBufAndCopyData(nPackageDataSize, nDataBufSize, &pRawDataBuf, pPackageData, nRecvDataSize))
                        goto clean;
					pPackageData = pIncr = pRawDataBuf;
					pIncr += nRecvDataSize;
					break;
				}

				if (nRecvDataSize >= nPackageDataSize)
				{	// if get enough data, we handle it.
					if (m_fsCallBack == fs_normal && HandleData != NULL)
					{
						HandleData(connSocket, *((SOCK_MESSAGE_HEADER*)pPackageData), pPackageData + ((SOCK_MESSAGE_HEADER*)pPackageData)->headerSize);
					}
					else if (m_fsCallBack == fs_stdBind && STD_HandleData != nullptr)
					{
						STD_HandleData(connSocket, *((SOCK_MESSAGE_HEADER*)pPackageData), pPackageData + ((SOCK_MESSAGE_HEADER*)pPackageData)->headerSize);
					}
					pPackageData = pPackageData + nPackageDataSize;
					nRecvDataSize -= nPackageDataSize;	// nRecvDataSize minus the handled data size.
					nPackageDataSize = 0;
					if (nRecvDataSize < sizeof(SOCK_MESSAGE_HEADER))
					{	// if recv data is less than package header size, we copy data and continue to recevie data.
						SAFE_COPY(pRawDataBuf, pPackageData, nRecvDataSize);
						pPackageData = pRawDataBuf;
						pIncr = pRawDataBuf + nRecvDataSize;
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
		{	// connection has closed. To do ... if we should set connSocket to INVALID_SOCKET
			connSocket = INVALID_SOCKET;
		}
		else
		{
			Log("RecvDataEx Error : %d \n", GetSocketErrorCode());
			connSocket = INVALID_SOCKET;
		}
	}

clean:  // Resource free.
	if (pRawDataBuf)
		delete[]pRawDataBuf;
}

bool CSimpleSocket::SendData(const socket_r& s, int dataType, const char* pData, int nDataSize)
{
	bool bRet = false;
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

		bRet = true;
		if (iResult == SOCKET_ERROR)
		{
			// log
			bRet = false;
		}
	}

	if (pAllocBuf)
		delete[]pAllocBuf;

	return bRet;
}

bool CSimpleSocket::SendData(int dataType, const char* pData, int nDataSize)
{
	return SendData(m_cltSocket, dataType, pData, nDataSize);
}

bool CSimpleSocket::StartNetwork(const std::string strIP, unsigned short uPort, NETWORK_PATTERN np, HandleDataFunc pHandleDataFunc)
{
	bool bRet = true;
	if (strIP.empty() || uPort == 0 || pHandleDataFunc == NULL || m_bConnected)
	{
		return false;
	}
	Log("Start network ... \n");
	m_fsCallBack = fs_normal;
	HandleData = pHandleDataFunc;
	m_workPattern = np;

#ifdef _WIN32
	if (m_workPattern == np_client || m_workPattern == np_client_ex)
#else
	if (m_workPattern == np_client)
#endif
	{
		std::thread thread_clt(&CSimpleSocket::CreateMsgClientTcp, this, strIP, uPort);
		thread_clt.detach();
	}
#ifdef _WIN32
	else if (m_workPattern == np_server || m_workPattern == np_server_ex)
#else
	else
#endif
	{
		std::thread thread_srv(&CSimpleSocket::CreateMsgServerTcp, this, strIP, uPort);
		thread_srv.detach();
	}

	return bRet;
}

bool CSimpleSocket::StartNetwork(const std::string strIP, unsigned short uPort, NETWORK_PATTERN np, STD_HandleDataFunc pHandleDataFunc)
{
	bool bRet = true;
	if (strIP.empty() || uPort == 0 || pHandleDataFunc == NULL || m_bConnected)
	{
		return false;
	}
	Log("Start network ... \n");
	m_fsCallBack = fs_stdBind;
	STD_HandleData = pHandleDataFunc;
	m_workPattern = np;

#ifdef _WIN32
	if (m_workPattern == np_client || m_workPattern == np_client_ex)
#else
	if (m_workPattern == np_client)
#endif
	{
		std::thread thread_clt(&CSimpleSocket::CreateMsgClientTcp, this, strIP, uPort);
		thread_clt.detach();
	}
#ifdef _WIN32
	else if (m_workPattern == np_server || m_workPattern == np_server_ex)
#else
	else
#endif
	{
		std::thread thread_srv(&CSimpleSocket::CreateMsgServerTcp, this, strIP, uPort);
		thread_srv.detach();
	}
	return bRet;
}

void CSimpleSocket::StopNetwork()
{
	m_bStop = true;
	std::lock_guard<std::mutex> mlock(m_mutexSocketInfos);

	if (m_workPattern != np_client){

		for (auto it = m_socketInfos.begin(); it != m_socketInfos.end(); it++)
		{
#ifdef _WIN32
			shutdown(it->first, SD_SEND);
#else
			shutdown(it->first, SHUT_RD);
			if(m_epfd){
				remove_fd(m_epfd, it->first);
			}
			if(it->second.pBuf){
				delete [](it->second.pBuf);
			}
#endif
			closesocket(it->first);
		}
		m_socketInfos.clear();
	}
	else{
		closesocket(m_cltSocket);
	}
}

bool CSimpleSocket::GetNetworkConnStatus()
{
	return m_bConnected;
}

void CSimpleSocket::SetSocketFailed(socket_r& s)
{
	std::lock_guard<std::mutex> mlock(m_mutexSocketInfos);
	auto iter = m_socketInfos.find(s);
	if (iter != m_socketInfos.end())
	{
		// (*iter).second.bAlive = false;
		// For now, just erase the socket record.
		
		if (iter->second.pBuf) {
			delete [](iter->second.pBuf);
		}
		
		m_socketInfos.erase(iter);
	}
}

void CSimpleSocket::SetLogCallBackFunc(LogFunc logFunc)
{
	if (logFunc)
	{
		CallBackLog = logFunc;
		Log("Set callback function success. \n");
	}
}

void CSimpleSocket::SetLogCallBackFunc(STD_LogFunc logFunc)
{
	if (logFunc)
	{
		STD_CallBackLog = logFunc;
		Log("Set std callback function success. \n");
	}
}

void CSimpleSocket::LogOutput(const char* pLogStr)
{
	if (CallBackLog != NULL)
	{
		CallBackLog(pLogStr);
	}
	else if (STD_CallBackLog != nullptr)
	{
		std::string strLog = pLogStr;
		STD_CallBackLog(strLog);
	}
	else
	{// default output debug info to DebugView(windows) or console(Linux)
#if defined(_win32)
		OutputDebugStringA(pLogStr);
#else
		printf(pLogStr);
#endif
	}
}

void CSimpleSocket::Log(const char* FormatStr, ...)
{
	char szLogStr[LOG_BUF_SIZE];
	va_list _Arglist;
	int _Ret = 0;

#if defined(_win32)
	_crt_va_start(_Arglist, FormatStr);
	// _Ret = _vswprintf_c_l(wszLogStr, sizeof(wszLogStr), FormatStr, NULL, _Arglist);
	_Ret = _vsprintf_c_l(wszLogStr, sizeof(szLogStr), FormatStr, NULL, _Arglist);
	_crt_va_end(_Arglist);
#else
	va_start(_Arglist, FormatStr);
	_Ret = vsnprintf(szLogStr, sizeof(szLogStr), FormatStr, _Arglist);
	va_end(_Arglist);
#endif
	
	if (_Ret > 0)
	{
		LogOutput(szLogStr);
	}
	else
	{
		LogOutput("Log: log error. \n");
	}
}

int CSimpleSocket::GetSocketErrorCode()
{
#ifdef _WIN32
	return GetLastError();
#else
	return errno;
#endif
}

#ifndef _WIN32
void CSimpleSocket::AddSocketInfo(const socket_r& connSock)
{
	SOCKET_INFO si;
	si.bAlive = true;
	si.nRecvSize = 0;
	si.pBuf = new char[BUF_SIZE];
	if (!si.pBuf) {
		Log("alloc memory failed. the connect socket is: %d", connSock);
	}
	std::lock_guard<std::mutex> mlock(m_mutexSocketInfos);
	m_socketInfos[connSock] = si;
}

void CSimpleSocket::DelSocketInfo(const socket_r& connSock)
{
	std::lock_guard<std::mutex> mlock(m_mutexSocketInfos);
	auto iter = m_socketInfos.find(connSock);
	if (iter != m_socketInfos.end()) {	
		if (iter->second.pBuf) {
			delete [](iter->second.pBuf);
		}
	}
}

int CSimpleSocket::set_nonblocking(int fd)
{
	int old_opt = fcntl(fd, F_GETFD);
	int new_opt = old_opt | O_NONBLOCK;
	fcntl(fd, F_SETFD, new_opt);
	return old_opt;
}

void CSimpleSocket::add_fd(int epollfd, int fd, bool enable_et)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;
	if(enable_et){
		event.events |= EPOLLET;
	}

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == INVALID_SOCKET) {
		Log("epoll_ctl: add fd: %d, error code : %d", fd, errno);
	}
}

void CSimpleSocket::remove_fd(int epollfd, int fd)
{
	if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0) == INVALID_SOCKET) {
		Log("epoll_ctl: remove fd %d, error code : %d", fd, errno);
	}
}

void CSimpleSocket::lt(epoll_event* events, int num, int epollfd, int listenfd)
{
    char buf[BUF_SIZE];
	for(int i = 0; i < num; i++){
		int sockfd = events[i].data.fd;
		if(sockfd == listenfd){
			struct sockaddr_in clientaddr;
			socklen_t addrlen = sizeof(clientaddr);
			int connSock = accept(listenfd, (struct sockaddr *) &clientaddr, &addrlen);
			if (connSock == INVALID_SOCKET) {
				Log("accept a new client failed, ip addr: %s, port: %d, error id: %d", inet_ntoa(clientaddr.sin_addr),
				 clientaddr.sin_port, errno);
			}			
			set_nonblocking(connSock);
			add_fd(epollfd, connSock, false);	// level triger
			AddSocketInfo(connSock);	// add the connect socket
		}else if(events[i].events & EPOLLIN){	// for small data which size less than 1K.
			Log("event trigger once\n");
			memset(buf, '\0', BUF_SIZE);
			int ret = recv(sockfd, buf, BUF_SIZE - 1, 0);
			if(ret <= 0){
				closesocket(sockfd);
				DelSocketInfo(sockfd);
				Log("recv error, error code: %d", GetSocketErrorCode());
				continue;
			}
		}else if(events[i].events & EPOLLOUT){
			// like EPOLLIN, we now just send a response. There will be a thread pool to write if need.
			SendData(1, "I get your message.", sizeof("I get your message."));
		}else if(events[i].events & EPOLLRDHUP)
		{
			// Log("");
			remove_fd(m_epfd, events[i].data.fd);
			closesocket(events[i].data.fd);
		}else if(events[i].events & EPOLLERR)
		{
			// Log("some error happenned, ip addr: %s, port: %d, error id: %d", inet_ntoa(clientaddr.sin_addr),
			// 	 clientaddr.sin_port, errno);
		}else{
			Log("something else happened \n");
		}
	}
}

void CSimpleSocket::et(epoll_event* events, int num, int epollfd, int listenfd)
{
	char buf[BUF_SIZE];
	for(int i = 0; i < num; i++){
		int sockfd = events[i].data.fd;
		if(sockfd == listenfd){
			struct sockaddr_in clientaddr;
			socklen_t addrlen = sizeof(clientaddr);
			int connSock = accept(listenfd, (struct sockaddr *) &clientaddr, &addrlen);
			if (connSock == INVALID_SOCKET) {
				Log("accept a new client failed, ip addr: %s, port: %d, error id: %d", inet_ntoa(clientaddr.sin_addr),
				 clientaddr.sin_port, errno);
			}			
			set_nonblocking(connSock);
			add_fd(epollfd, connSock, true);	// edge triger
			AddSocketInfo(connSock);	// add the connect socket
		}else if(events[i].events & EPOLLIN){
			// here we will call a thread pool to handle the data. now we just create a tmp thread. To do ... 
			// int conn = events[i].data.fd;
			// std::thread th_handler(&CSimpleSocket::RecvDataEx, this, conn);
			// th_handler.detach();
		}else if(events[i].events & EPOLLOUT){
			// like EPOLLIN, we now just send a response. There will be a thread pool to write if need.
			SendData(1, "I get your message.", sizeof("I get your message."));
		}else if(events[i].events & EPOLLRDHUP)
		{
			// Log("");
			remove_fd(m_epfd, events[i].data.fd);
			closesocket(events[i].data.fd);
		}else if(events[i].events & EPOLLERR)
		{
			// Log("some error happenned, ip addr: %s, port: %d, error id: %d", inet_ntoa(clientaddr.sin_addr),
			// 	 clientaddr.sin_port, errno);
		}else{
			Log("something else happened \n");
		}
	}
}

void CSimpleSocket::handle_epoll_event(epoll_event* events, int num, int epollfd, int listenfd)
{	
	for(int i = 0; i < num; ++i)
	{	
		if (events[i].data.fd == listenfd) {
			struct sockaddr_in clientaddr;
			socklen_t addrlen = sizeof(clientaddr);
			int connSock = accept(listenfd, (struct sockaddr *) &clientaddr, &addrlen);
			if (connSock == INVALID_SOCKET) {
				Log("accept a new client. ");
			}			
			set_nonblocking(connSock);
			if(m_workPattern == np_server_epoll_et){
				add_fd(m_epfd, connSock);
			}
			else if(m_workPattern == np_server_epoll_lt){
				add_fd(m_epfd, connSock, false);
			}
		}
		else if (events[i].events & EPOLLET) {
			// et_single();
			Log("");
		}
		else{
			// lt_single();
			Log("");
		}
	}
}

#endif

}// SIMPLE_SOCKET namespace end

