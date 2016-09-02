#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>

#include "StreamQueue.h"
#include "NPacket.h"
#include "Protocol.h"
#include "Chat_Server.h"

CLIENT g_mClient;
ROOM g_mChatRoom;
SOCKET listen_sock;
static DWORD g_dwClientID = 1;
static DWORD g_dwChatRoomID = 1;

void main()
{
	CNPacket::_ValueSizeCheck();

	if (!InitServer())
		return;
	
	if (!OpenServer())
		return;

	while (1)
	{
		NetWork();
	}
}

/*--------------------------------------------------------------------------------------*/
// 서버 초기화
/*--------------------------------------------------------------------------------------*/
BOOL InitServer()
{
	int retval;

	printf("Server Initializing....\t");

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

	printf("OK\n");
	return TRUE;
}

/*--------------------------------------------------------------------------------------*/
// 서버 on
/*--------------------------------------------------------------------------------------*/
BOOL OpenServer()
{
	int retval;

	printf("Server Opening.....\t");

	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)	return FALSE;

	printf("OK\n");
	return TRUE;
}

/*--------------------------------------------------------------------------------------*/
// main NetWork
/*--------------------------------------------------------------------------------------*/
void NetWork()
{
	int retval;
	CLIENTIter iter;

	FD_SET ReadSet, WriteSet;
	FD_ZERO(&ReadSet);
	FD_ZERO(&WriteSet);

	FD_SET(listen_sock, &ReadSet);

	/*--------------------------------------------------------------------------------------*/
	// ReadSet, WriteSet 등록
	/*--------------------------------------------------------------------------------------*/
	for (iter = g_mClient.begin(); iter != g_mClient.end(); ++iter)
	{
		FD_SET(iter->second->sock, &ReadSet);
		
		if (iter->second->SendQ.GetUseSize() > 0)
			FD_SET(iter->second->sock, &WriteSet);
	}

	TIMEVAL Time;
	Time.tv_sec = 0;
	Time.tv_usec = 0;

	retval = select(0, &ReadSet, &WriteSet, NULL, &Time);

	if (retval > 0)
	{
		//Accept
		if (FD_ISSET(listen_sock, &ReadSet))
			AcceptClient();
			
		else
			SocketProc(ReadSet, WriteSet);
	}
}

void AcceptClient()
{
	CLIENTIter iter;
	WCHAR wcAddr[16];

	int addrlen = sizeof(SOCKADDR_IN);
	g_mClient.insert(pair<DWORD, stClient*>(g_dwClientID, new stClient));
	iter = g_mClient.find(g_dwClientID);
	iter->second->dwClientNo = g_dwClientID++;

	if (iter != g_mClient.end())
	{
		memset(&iter->second->cClientName, '\0', sizeof(iter->second->cClientName));
		iter->second->sock = accept(listen_sock, (SOCKADDR *)&(iter->second->sockAddr), &addrlen);
	}

	InetNtop(AF_INET, &iter->second->sockAddr.sin_addr, wcAddr, sizeof(wcAddr));
	wprintf(L"Accept - %s:%d [UserNo:%d]\n", wcAddr,
		iter->second->sockAddr.sin_port, g_dwClientID);
}

/*--------------------------------------------------------------------------------------*/
// 해당 socket 처리
/*--------------------------------------------------------------------------------------*/
void SocketProc(FD_SET ReadSet, FD_SET WriteSet)
{
	CLIENTIter iter;

	for (iter = g_mClient.begin(); iter != g_mClient.end(); ++iter)
	{
		if (FD_ISSET(iter->second->sock, &WriteSet))
			WriteProc(iter->first);

		if (FD_ISSET(iter->second->sock, &ReadSet))
			RecvProc(iter->first);
	}
}

/*--------------------------------------------------------------------------------------*/
// Receive Process
/*--------------------------------------------------------------------------------------*/
BOOL RecvProc(DWORD dwClientNo)
{
	int retval;
	CLIENTIter iter = g_mClient.find(dwClientNo);

	retval = recv(iter->second->sock, iter->second->RecvQ.GetWriteBufferPtr(),
			iter->second->RecvQ.GetFreeSize(), 0);
	iter->second->RecvQ.MoveWritePos(retval);

	if (retval > 0)
		PacketProc(dwClientNo, iter->second);

	return TRUE;
}

/*--------------------------------------------------------------------------------------*/
// Send Process
/*--------------------------------------------------------------------------------------*/
void WriteProc(DWORD dwClientNo)
{
	int retval;
	CLIENTIter iter = g_mClient.find(dwClientNo);

	if (iter->second->SendQ.GetUseSize() > 0)
	{
		retval = send(iter->second->sock, (char *)iter->second->SendQ.GetReadBufferPtr(),
			iter->second->SendQ.GetUseSize(), 0);
		iter->second->SendQ.RemoveData(retval);
	}
}

void PacketProc(DWORD dwClientNo, stClient * pClient)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	/*--------------------------------------------------------------------------------------*/
	//RecvQ 용량이 header보다 작은지 검사
	/*--------------------------------------------------------------------------------------*/
	if (pClient->RecvQ.GetUseSize() < sizeof(st_PACKET_HEADER))
		return;

	pClient->RecvQ.Peek((char *)&header, sizeof(st_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//header + payload 용량이 RecvQ용량보다 큰지 검사
	/*--------------------------------------------------------------------------------------*/
	if (pClient->RecvQ.GetUseSize() < header.wPayloadSize + sizeof(st_PACKET_HEADER))
		return;

	pClient->RecvQ.RemoveData(sizeof(st_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//payload를 cPacket에 뽑고 같은지 검사
	/*--------------------------------------------------------------------------------------*/
	if (header.wPayloadSize != pClient->RecvQ.Get((char *)cPacket.GetBufferPtr(), header.wPayloadSize))
		return;

	wprintf(L"PacketRecv [UserNo:%d][Type:%d]\n", dwClientNo, header.wMsgType);

	/*--------------------------------------------------------------------------------------*/
	// Message 타입에 따른 Packet 처리
	/*--------------------------------------------------------------------------------------*/
	switch (header.wMsgType)
	{
	case df_REQ_LOGIN:
		packetProc_ReqLogin(pClient, &cPacket);
		break;

	case df_REQ_ROOM_LIST:
		packetProc_ReqRoomList(pClient, &cPacket);
		break;

	case df_REQ_ROOM_CREATE:
		packetProc_ReqRoomCreate(pClient, &cPacket);
		break;

	case df_REQ_ROOM_ENTER:
		packetProc_ReqRoomEnter(pClient, &cPacket);
		break;

	case df_REQ_CHAT:
		packetProc_ReqChat(pClient, &cPacket);
		break;

	case df_REQ_ROOM_LEAVE:
		packetProc_ReqRoomLeave(pClient, &cPacket);
		break;
	}
}

/*--------------------------------------------------------------------*/
// Client -> Server
/*--------------------------------------------------------------------*/

//	Client -> Server Login
BOOL packetProc_ReqLogin(stClient *pClient, CNPacket *cPacket)
{
	CLIENTIter iter;
	BYTE byResult = df_RESULT_LOGIN_OK;

	WCHAR cNickname[15];
	*cPacket >> cNickname;

	for (iter = g_mClient.begin(); iter != g_mClient.end(); ++iter)
	{
		//중복 닉네임
		if (0 == wcscmp(iter->second->cClientName, cNickname))
			byResult = df_RESULT_LOGIN_DNICK;
	}

	if (byResult == 1)	memcpy(pClient->cClientName, cNickname, dfNICK_MAX_LEN * 2);
	
	return packetProc_ResLogin(pClient, byResult);
}

//----------------------------------------------------------------------
// Client -> Server RoomList
//----------------------------------------------------------------------
BOOL packetProc_ReqRoomList(stClient *pClient, CNPacket *cPacket)
{
	return packetProc_ResRoomList(pClient);
}

//----------------------------------------------------------------------
// Client -> Server RoomCreate
//----------------------------------------------------------------------
BOOL packetProc_ReqRoomCreate(stClient *pClient, CNPacket *cPacket)
{
	BYTE byResult = df_RESULT_ROOM_CREATE_OK;
	short wRoomNameSize;
	*cPacket >> wRoomNameSize;

	WCHAR *pRoomName = new WCHAR[(wRoomNameSize/2) + 1];
	*cPacket >> pRoomName;
	pRoomName[(wRoomNameSize / 2)] = '\0';

	stChatRoom *stRoom = new stChatRoom;
	stRoom->dwRoomNo = g_dwChatRoomID++;
	wcscpy_s(stRoom->cRoomName, pRoomName);
	g_mChatRoom.insert(pair<DWORD, stChatRoom*>(stRoom->dwRoomNo, stRoom));

	if (packetProc_ResRoomCreate(pClient, byResult, stRoom->dwRoomNo,
		wRoomNameSize + 2, stRoom->cRoomName))
	{
		wprintf(L"Create Room [UserNo:%d][Room:%s] [TotalRoom:%d]\n",
			pClient->dwClientNo, stRoom->cRoomName, g_mChatRoom.size());
		return TRUE;
	}

	else	return FALSE;
}

//----------------------------------------------------------------------
// Client -> Server RoomEnter
//----------------------------------------------------------------------
BOOL packetProc_ReqRoomEnter(stClient *pClient, CNPacket *cPacket)
{
	ROOMIter roomIter;

	pair<map<DWORD, stClient *>::iterator, bool> pr;

	BYTE byResult;
	int dwRoomNum;

	*cPacket >> dwRoomNum;

	roomIter = g_mChatRoom.find(dwRoomNum);
	pr = roomIter->second->mRoomClient.insert(pair<DWORD, stClient *>
		(pClient->dwClientNo, pClient));
	pClient->dwChatRoomNo = dwRoomNum;

	if (TRUE == pr.second)		byResult = df_RESULT_ROOM_ENTER_OK;
	else						byResult = df_RESULT_ROOM_ENTER_ETC;

	if (packetProc_ResRoomEnter(pClient, dwRoomNum, byResult))
	{
		packetProc_ResUserEnter(pClient);
		printf("Enter Room [Room:%s][UserNo:%d]\n", roomIter->second->cRoomName,
			pClient->dwClientNo);
	}

	return TRUE;
}

//----------------------------------------------------------------------
// Client -> Server Chat
//----------------------------------------------------------------------
BOOL packetProc_ReqChat(stClient *pClient, CNPacket *cPacket)
{
	short wMsgSize;
	*cPacket >> wMsgSize;

	WCHAR *pMessage = new WCHAR[wMsgSize];
	*cPacket >> pMessage;

	return packetProc_ResChat(pClient, wMsgSize, pMessage);
}

//----------------------------------------------------------------------
// Client -> Server RoomLeave
//----------------------------------------------------------------------
BOOL packetProc_ReqRoomLeave(stClient *pClient, CNPacket *cPacket)
{
	ROOMIter roomIter;

	roomIter = g_mChatRoom.find(pClient->dwChatRoomNo);
	roomIter->second->mRoomClient.erase(pClient->dwClientNo);
	pClient->dwChatRoomNo = 0;

	return packetProc_ResRoomLeave(pClient, roomIter->second);
}
/*--------------------------------------------------------------------*/


/*--------------------------------------------------------------------*/
// Server -> Client
/*--------------------------------------------------------------------*/

//----------------------------------------------------------------------
// Server -> Client Login
//----------------------------------------------------------------------
BOOL packetProc_ResLogin(stClient *pClient, BYTE byResult)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	cPacket << byResult;
	cPacket << (unsigned int)pClient->dwClientNo;

	header.byCode = dfPACKET_CODE;
	header.wMsgType = df_RES_LOGIN;
	header.wPayloadSize = (WORD)cPacket.GetDataSize();
	header.byCheckSum = MakeCheckSum(df_RES_LOGIN, &cPacket);

	SendUnicast(pClient, &header, &cPacket);
	return TRUE;
}

//----------------------------------------------------------------------
// Server -> Client RoomList
//----------------------------------------------------------------------
BOOL packetProc_ResRoomList(stClient *pClient)
{
	ROOMIter roomIter;
	CLIENTIter clientIter;

	CNPacket cPacket;
	st_PACKET_HEADER header;

	WORD wRoomCount = g_mChatRoom.size();
	cPacket << (short)wRoomCount;

	for (roomIter = g_mChatRoom.begin(); roomIter != g_mChatRoom.end(); ++roomIter)
	{
		cPacket << (unsigned int)roomIter->second->dwRoomNo;
		cPacket << (short)((wcslen(roomIter->second->cRoomName) + 1) * 2);
		cPacket << roomIter->second->cRoomName;

		for (clientIter = roomIter->second->mRoomClient.begin();
			clientIter != roomIter->second->mRoomClient.end(); ++clientIter)
			cPacket << clientIter->second->cClientName;
	}

	header.byCode = dfPACKET_CODE;
	header.wMsgType = df_RES_ROOM_LIST;
	header.wPayloadSize = (WORD)cPacket.GetDataSize();
	header.byCheckSum = MakeCheckSum(df_RES_ROOM_LIST, &cPacket);

	SendUnicast(pClient, &header, &cPacket);

	return TRUE;
}

//----------------------------------------------------------------------
// Server -> Client RoomCreate
//----------------------------------------------------------------------
BOOL packetProc_ResRoomCreate(stClient *pClient, BYTE byResult, DWORD dwRoomNo,
	WORD wRoomNameSize, WCHAR *cRoomName)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	cPacket << byResult;
	cPacket << (unsigned int)dwRoomNo;
	cPacket << (short)wRoomNameSize;
	cPacket << cRoomName;

	header.byCode = dfPACKET_CODE;
	header.wMsgType = df_RES_ROOM_CREATE;
	header.wPayloadSize = (WORD)cPacket.GetDataSize();
	header.byCheckSum = MakeCheckSum(df_RES_ROOM_CREATE, &cPacket);

	SendBroadcast(&header, &cPacket);

	return TRUE;
}

//----------------------------------------------------------------------
// Server -> Client RoomEnter
//----------------------------------------------------------------------
BOOL packetProc_ResRoomEnter(stClient *pClient, DWORD dwRoomNum, BYTE byResult)
{
	ROOMIter roomIter;
	CLIENTIter clientIter;

	st_PACKET_HEADER header;
	CNPacket cPacket;

	cPacket << byResult;

	if (df_RESULT_ROOM_ENTER_OK == byResult)
	{
		roomIter = g_mChatRoom.find(dwRoomNum);
		cPacket << (unsigned int)roomIter->second->dwRoomNo;
		cPacket << (short)((wcslen(roomIter->second->cRoomName) + 1) * 2);
		cPacket << roomIter->second->cRoomName;

		cPacket << (BYTE)roomIter->second->mRoomClient.size();
		for (clientIter = roomIter->second->mRoomClient.begin();
			clientIter != roomIter->second->mRoomClient.end(); ++clientIter)
		{
			WCHAR *wNickName = new WCHAR[15];
			memset(wNickName, '\0', dfNICK_MAX_LEN * 2);
			memcpy(wNickName, clientIter->second->cClientName, 
				wcslen(clientIter->second->cClientName)*2);

			cPacket.PutData((BYTE *)wNickName, dfNICK_MAX_LEN*2);
			cPacket << (unsigned int)clientIter->second->dwClientNo;
		}
	}

	header.byCode = dfPACKET_CODE;
	header.wMsgType = df_RES_ROOM_ENTER;
	header.wPayloadSize = (WORD)cPacket.GetDataSize();
	header.byCheckSum = MakeCheckSum(df_RES_ROOM_ENTER, &cPacket);

	SendUnicast(pClient, &header, &cPacket);

	return TRUE;
}

//----------------------------------------------------------------------
// Server -> Client Chat
//----------------------------------------------------------------------
BOOL packetProc_ResChat(stClient *pClient, WORD wMsgSize, WCHAR *pMessage)
{
	ROOMIter roomIter;
	
	st_PACKET_HEADER header;
	CNPacket cPacket;

	roomIter = g_mChatRoom.find(pClient->dwChatRoomNo);

	cPacket << (unsigned int)pClient->dwClientNo;
	cPacket << (short)wMsgSize;
	cPacket << pMessage;

	header.byCode = dfPACKET_CODE;
	header.wMsgType = df_RES_CHAT;
	header.wPayloadSize = (WORD)cPacket.GetDataSize();
	header.byCheckSum = MakeCheckSum(df_RES_CHAT, &cPacket);

	SendBroadcastForRoom(roomIter->second, pClient, &header, &cPacket);
	return TRUE;
}

//----------------------------------------------------------------------
// Server -> Client Room Leave
//----------------------------------------------------------------------
BOOL packetProc_ResRoomLeave(stClient *pClient, stChatRoom *pChatRoom)
{
	st_PACKET_HEADER header;
	CNPacket cPacket;

	cPacket << (unsigned int)pClient->dwClientNo;

	header.byCode = dfPACKET_CODE;
	header.wMsgType = df_RES_ROOM_LEAVE;
	header.wPayloadSize = (WORD)cPacket.GetDataSize();
	header.byCheckSum = MakeCheckSum(df_RES_ROOM_LEAVE, &cPacket);

	SendUnicast(pClient, &header, &cPacket);
	SendBroadcastForRoom(pChatRoom, pClient, &header, &cPacket);
	return TRUE;
}

//----------------------------------------------------------------------
// Server -> Client Room Delete
//----------------------------------------------------------------------
BOOL packetProc_ResRoomDelete()
{
	return TRUE;
}

//----------------------------------------------------------------------
// Server -> Client User Enter
//----------------------------------------------------------------------
BOOL packetProc_ResUserEnter(stClient *pClient)
{
	ROOMIter roomIter;

	st_PACKET_HEADER header;
	CNPacket cPacket;

	roomIter = g_mChatRoom.find(pClient->dwChatRoomNo);

	WCHAR *wNickName = new WCHAR[15];
	memset(wNickName, '\0', dfNICK_MAX_LEN * 2);
	memcpy(wNickName, pClient->cClientName, wcslen(pClient->cClientName) * 2);

	cPacket.PutData((BYTE *)wNickName, dfNICK_MAX_LEN * 2);
	cPacket << (unsigned int)pClient->dwClientNo;

	header.byCode = dfPACKET_CODE;
	header.wMsgType = df_RES_USER_ENTER;
	header.wPayloadSize = (WORD)cPacket.GetDataSize();
	header.byCheckSum = MakeCheckSum(df_RES_USER_ENTER, &cPacket);

	SendBroadcastForRoom(roomIter->second, pClient, &header, &cPacket);
	return TRUE;
}
/*--------------------------------------------------------------------*/


/*--------------------------------------------------------------------*/
// 1 : 1 Send
/*--------------------------------------------------------------------*/
void SendUnicast(stClient *pClient, st_PACKET_HEADER *header, CNPacket *cPacket)
{
	pClient->SendQ.Put((char *)header, sizeof(st_PACKET_HEADER));
	pClient->SendQ.Put((char *)cPacket->GetBufferPtr(), cPacket->GetDataSize());
}

/*--------------------------------------------------------------------*/
// 1 : n Send
/*--------------------------------------------------------------------*/
void SendBroadcast(st_PACKET_HEADER *header, CNPacket *cPacket)
{
	CLIENTIter iter;

	for (iter = g_mClient.begin(); iter != g_mClient.end(); ++iter)
	{
		SendUnicast(iter->second, header, cPacket);
	}
}

/*--------------------------------------------------------------------*/
// 1 : n Send for Room
/*--------------------------------------------------------------------*/
void SendBroadcastForRoom(stChatRoom *pChatRoom, stClient *pClient, 
	st_PACKET_HEADER *header, CNPacket *cPacket)
{
	CLIENTIter iter;

	for (iter = g_mClient.begin(); iter != g_mClient.end(); ++iter)
	{
		if (iter->second->dwChatRoomNo == pChatRoom->dwRoomNo &&
			iter->second != pClient)
			SendUnicast(iter->second, header, cPacket);
	}
}

/*--------------------------------------------------------------------*/
// CheckSum 만들기
/*--------------------------------------------------------------------*/
BYTE MakeCheckSum(WORD wMsgType, CNPacket *cPacket)
{
	BYTE byCheckSum;

	byCheckSum = wMsgType;

	for (int iCnt = 0; iCnt < cPacket->GetDataSize(); iCnt++)
		byCheckSum += cPacket->GetBufferPtr()[iCnt];

	return byCheckSum;
}