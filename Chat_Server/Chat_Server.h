#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

struct stUser
{
	WCHAR cUserName[15];
	SOCKADDR_IN sockAddr;
	SOCKET sock;
};

#define USER map<int, stUser *>
#define USERIter USER::iterator