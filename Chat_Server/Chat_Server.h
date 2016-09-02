#pragma comment(lib, "Ws2_32.lib")

using namespace std;

struct stClient
{
	WCHAR cClientName[15];
	DWORD dwClientNo;
	DWORD dwChatRoomNo;

	SOCKADDR_IN sockAddr;
	SOCKET sock;

	CAyaStreamSQ SendQ;
	CAyaStreamSQ RecvQ;
};

#define CLIENT map<DWORD, stClient *>
#define CLIENTIter CLIENT::iterator

struct stChatRoom
{
	WCHAR cRoomName[1000];
	DWORD dwRoomNo;

	CLIENT mRoomClient;
};

#define ROOM map<DWORD, stChatRoom *>
#define ROOMIter ROOM::iterator


BOOL InitServer();
BOOL OpenServer();
void NetWork();

BOOL RecvProc(DWORD dwClientNo);
void WriteProc(DWORD dwClientNo);

void AcceptClient();
void SocketProc(FD_SET ReadSet, FD_SET WriteSet);

void PacketProc(DWORD dwClientNo, stClient * pClient);

/*--------------------------------------------------------------------*/
// Client -> Server
/*--------------------------------------------------------------------*/
BOOL packetProc_ReqLogin(stClient *pClient, CNPacket *cPacket);
BOOL packetProc_ReqRoomList(stClient *pClient, CNPacket *cPacket);
BOOL packetProc_ReqRoomCreate(stClient *pClient, CNPacket *cPacket);
BOOL packetProc_ReqRoomEnter(stClient *pClient, CNPacket *cPacket);
BOOL packetProc_ReqChat(stClient *pClient, CNPacket *cPacket);
BOOL packetProc_ReqRoomLeave(stClient *pClient, CNPacket *cPacket);

/*--------------------------------------------------------------------*/
// Server -> Client
/*--------------------------------------------------------------------*/
BOOL packetProc_ResLogin(stClient *pClient, BYTE byResult);
BOOL packetProc_ResRoomList(stClient *pClient);
BOOL packetProc_ResRoomCreate(stClient *pClient, BYTE byResult, DWORD dwRoomNo,
	WORD wRoomNameSize, WCHAR *cRoomName);
BOOL packetProc_ResRoomEnter(stClient *pClient, DWORD dwRoomNum, BYTE byResult);
BOOL packetProc_ResChat(stClient *pClient, WORD wMsgSize, WCHAR *pMessage);
BOOL packetProc_ResRoomLeave(stClient *pClient, stChatRoom *pChatRoom);
BOOL packetProc_ResRoomDelete();
BOOL packetProc_ResUserEnter(stClient *pClient);

/*--------------------------------------------------------------------*/
// Send ÇÔ¼ö
/*--------------------------------------------------------------------*/
void SendUnicast(stClient *pClient, st_PACKET_HEADER *header, CNPacket *cPacket);
void SendBroadcast(st_PACKET_HEADER *header, CNPacket *cPacket);
void SendBroadcastForRoom(stChatRoom *pChatRoom, stClient *pClient, 
	st_PACKET_HEADER *header, CNPacket *cPacket);

BYTE MakeCheckSum(WORD wMsgType, CNPacket *cPacket);
