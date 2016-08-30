#include "Chat_Server.h"
#include "Protocol.h"
#include "NPacket.h"

USER mUser;
SOCKET listen_sock;
static int iUserID = 1;
CNPacket SendQ, RecvQ;

BOOL InitServer();
BOOL OpenServer();
void NetWork();
void RecvProc(SOCKET sock);

BYTE MakeCheckSum(BYTE MsgType, int iPayloadSize, BYTE* pPayload);

void main()
{
	CNPacket::_ValueSizeCheck();

	printf("Server Initializing....\t");
	if (!InitServer())
		return;
	printf("OK\n");

	printf("Server Opening.....\t");
	if (!OpenServer())
		return;
	printf("OK\n");

	while (1)
	{
		NetWork();
	}
}

BOOL InitServer()
{
	int retval;

	//윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return FALSE;

	//SOCKET 생성
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
		return FALSE;

	//Bind
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(dfNETWORK_PORT);
	retval = bind(listen_sock, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
	if (retval == SOCKET_ERROR)
		return FALSE;

	return TRUE;
}

BOOL OpenServer()
{
	int retval;

	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)	return FALSE;

	return TRUE;
}

void NetWork()
{
	int retval;
	USERIter iter;
	char buf[100];

	FD_SET ReadSet;
	FD_ZERO(&ReadSet);
	FD_SET(listen_sock, &ReadSet);

	for (iter = mUser.begin(); iter != mUser.end(); ++iter)
	{
		FD_SET(iter->second->sock, &ReadSet);
	}

	TIMEVAL Time;
	Time.tv_sec = 0;
	Time.tv_usec = 0;

	retval = select(0, &ReadSet, NULL, NULL, &Time);

	if (retval > 0)
	{
		if (FD_ISSET(listen_sock, &ReadSet))
		{
			printf("con\n");
			int addrlen = sizeof(SOCKADDR_IN);
			mUser.insert(pair<int, stUser*>(iUserID, new stUser));
			iter = mUser.find(iUserID);
			if (iter != mUser.end())
			{
				memset(&iter->second->cUserName, '\0', sizeof(iter->second->cUserName));
				iter->second->sock = accept(listen_sock, (SOCKADDR *)&(iter->second->sockAddr), &addrlen);
			}
		}

		for (iter = mUser.begin(); iter != mUser.end(); ++iter)
		{
			if (FD_ISSET(iter->second->sock, &ReadSet))
			{
				RecvProc(iter->second->sock);
			}
		}
	}
}

void RecvProc(SOCKET sock)
{
	int retval;
	st_PACKET_HEADER *header = new st_PACKET_HEADER;

	retval = recv(sock, (char *)header, sizeof(st_PACKET_HEADER), 0);

	if (retval < sizeof(header))
		//접속 끊기

	//else if (header.byCode != dfPACKET_CODE ||
	//	retval - sizeof(header) < header.wPayloadSize)
	//	break;

	switch (header->wMsgType)
	{
	case df_REQ_LOGIN:
		break;

	case df_REQ_ROOM_LIST:
		break;

	case df_REQ_ROOM_CREATE:
		break;

	case df_REQ_ROOM_ENTER:
		break;

	case df_REQ_CHAT:
		break;

	case df_REQ_ROOM_LEAVE:
		break;
	}
}

BYTE MakeCheckSum(BYTE MsgType, int iPayloadSize, BYTE* pPayload)
{
	BYTE byCheckSum;

	byCheckSum = MsgType;

	for (int iCnt = 0; iCnt < iPayloadSize; iCnt++)
		byCheckSum += pPayload[iCnt];

	return byCheckSum;
}