#pragma comment(lib, "ws2_32")
#include "stdafx.h"
#include "Protocol.h"
#include "CRingBuffer.h"
#include "CMessage.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

typedef struct st_SESSION
{
	SOCKET socket;
	SOCKADDR_IN clientaddr;
	DWORD ID;
	BOOL bIsLogin;
	CRingBuffer RecvQ;
	CRingBuffer SendQ;

	st_SESSION(void)
	{
		socket = INVALID_SOCKET;
		ZeroMemory(&clientaddr, sizeof(clientaddr));
		bIsLogin = FALSE;
		RecvQ.ClearBuffer();
		SendQ.ClearBuffer();
	}
	~st_SESSION(void) {}
} SESSION;

typedef struct st_CLIENT
{
	UINT64 ID;
	WCHAR NickName[dfNICK_MAX_LEN];

	st_CLIENT(void)
	{
		ID = 0;
		NickName[dfNICK_MAX_LEN] = { '\0' };
	}
	~st_CLIENT(void) {}
} CLIENT;

// 리슨소켓
SOCKET listen_socket = INVALID_SOCKET;
// select모델의 대기시간
timeval selecttime = { 0,0 };
// 에러 로그를 출력할 파일 포인터
FILE* g_File;
// 에러 로그 파일의 제목
WCHAR g_TextTitle[64] = { 0 };
// 세션 ID
int g_iSessionID = 1;
// 클라이언트 ID
int g_iClientID = 1;
// 로그인한 클라이언트의 ID
int g_iLoginClientID = 0;
int g_JsonSize = 0;

// 서버에 접속된 모든 client를 관리하는 Map
std::map<DWORD,SESSION*> ClientList;
// 회원가입된 회원을 관리하는 Map
std::unordered_map<UINT64, CLIENT*> MemberMap;
// 가입된 회원들의 친구리스트를 관리하는 multimap
std::multimap<UINT64, CLIENT*> MemberFriendMap;
// 특정 회원이 특정회원에게 보낸 친구요청 리스트를 관리하는 multimap, first = 요청한 회원의 ID, second = 요청받은 회원의 CLIENT*
std::multimap<UINT64, CLIENT*> MemberReqFriendMap;
// 특정 회원이 특정회원에게 받은 친구요청 리스트를 관리하는 multimap, first = 요청 받은 회원의 ID, second = 요청한 회원의 CLIENT*
std::multimap<UINT64, CLIENT*> MemberResFriendMap;

// 네트워크 초기화
bool InitialNetwork(void);
// 네트워크 로직이 돌아가는 함수(select)
void NetWorkPart(void);
// 네트워크 로직 안에서 실질적으로 select함수를 호출하는 함수
void SelectSocket(DWORD* dwTableNO, SOCKET* pTableSocket, FD_SET* pReadSet, FD_SET* pWriteSet);
// 클라이언트의 connect() 요청을 accpet()하는 함수
void Accept();
// 해당 소켓에 recv하는 함수
void NetWork_Recv(DWORD UserID);
// 접속 종료 절차
void Disconnect(DWORD UserID);
// 실질적으로 ClientList함수에서 session을 삭제하는 함수
void DeleteClient();
// 패킷 생성과 처리 절차
int CompletePacket(SESSION* session);
// 해당 소켓이 send하는 함수
void NetWork_Send(DWORD UserID);
// 실질적인 패킷 처리 함수
bool PacketProc(SESSION* session, WORD dwType, CMessage* message);

// 현재까지의 모든 데이터들을 json을 이용해 파일형태로 저장하는 함수
void SaveAllData(void);

// 세이브된 데이터를 가져오는 함수
void LoadAllData(void);

// 패킷 대응 함수
// 클라이언트의 요청
bool NetWork_ReqRegister(SESSION* session, CMessage* message);
bool NetWork_ReqLogin(SESSION* session, CMessage* message);
bool NetWork_ReqAccountList(SESSION* session);
bool NetWork_ReqFriendList(SESSION* session);
bool NetWork_ReqFriendReqList(SESSION* session);
bool NetWork_ReqFriendResList(SESSION* session);
bool NetWork_ReqFriendRemove(SESSION* session, CMessage* message);
bool NetWork_ReqFriendReq(SESSION* session, CMessage* message);
bool NetWork_ReqFriendRedRemove(SESSION* session, CMessage* message);
bool NetWork_ReqFriendReqDeny(SESSION* session, CMessage* message);
bool NetWork_ReqFriendReqAgree(SESSION* session, CMessage* message);
bool NetWork_ReqEcho(SESSION* session, CMessage* message);

// 서버의 응답
void NetWork_ResRegister(SESSION* session, WORD Result, UINT64 pClientID);
void NetWork_ResLogin(SESSION* session, CLIENT* pSession);
void NetWork_ResAccountList(SESSION* session);
void NetWork_ResFriendList(SESSION* session);
void NetWork_ResFriendReqList(SESSION* session);
void NetWork_ResFriendResList(SESSION* session);
void NetWork_ResFriendRemove(SESSION* session, BYTE result, UINT64 ClientID);
void NetWork_ResFriendReq(SESSION* session, BYTE result, UINT64 ClientID);
void NetWork_ResFriendReqRemove(SESSION* session, BYTE result, UINT64 ClientID);
void NetWork_ResFriendReqDeny(SESSION* session, BYTE result, UINT64 ClientID);
void NetWork_ResFriendReqAgree(SESSION* session, BYTE result, UINT64 ClientID);
void NetWork_ResEcho(SESSION* session, CMessage* message);

// 패킷 만드는 함수
void MakePacket_ResRegister(st_PACKET_HEADER* pHeader, CMessage* message, WORD result, UINT64 ID);
void MakePacket_ResLogin(st_PACKET_HEADER* pHeader, CMessage* message, CLIENT* pSession);
void MakePacket_ResAccountList(st_PACKET_HEADER* pHeader, CMessage* message);
void MakePacket_ResFriendList(st_PACKET_HEADER* pHeader, CMessage* message, BOOL isLogined);
void MakePacket_ResFriendReqList(st_PACKET_HEADER* pHeader, CMessage* message, BOOL isLogined);
void MakePacket_ResFriendResList(st_PACKET_HEADER* pHeader, CMessage* message, BOOL isLogined);
void MakePacket_ResFriendRemove(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID);
void MakePacket_ResFriendReq(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID);
void MakePacket_ResFriendReqRemove(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID);
void MakePacket_ResFriendReqDeny(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID);
void MakePacket_ResFriendReqAgree(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID);
void MakePacket_ResEcho(st_PACKET_HEADER* pHeader, CMessage* message, CMessage* text);

// ClientList에서 ID를 key로 SESSION*를 반환하는 함수
SESSION* FindSession(DWORD id);
// ClientList에 새로운 Client를 삽입하는 함수
void AddSession(SESSION* session);
//// ClientList에서 삭제하는 함수
//SESSION* DeleteSession(DWORD id);

// MemberMap에서 회원의 닉네임을 찾아주는 함수
CLIENT* FindMember(UINT64 id);
// 새로운 회원을 추가하는 함수
void AddMemeber(UINT64 id, CLIENT* pClient);
// MemberMap에서 회원을 삭제하는 함수
void DeleteMember(UINT64 id);

// MemberFriendMap에서 특정 Member의 친구 iterator를 받는 함수
std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> FindMemberFriend(UINT64 id);
// MemberFriendMap에 특정 Member의 친구를 추가하는 함수
void AddMemberFriend(UINT64 MainFriend, UINT64 TargetFriend);
// MemberFriendMap에서 특정 Member의 친구 중 1명을 삭제하는 함수
void DeleteMemberFriend(UINT64 MainFriend, UINT64 TargetFriend);

// MemberReqFriendMap에서 특정 Member의 친구 추가 요청리스트의 iterator를 받는 함수
std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> FindMemberFriendReq(UINT64 id);
// MemberReqFriendMap에 특정 Member의 친구요청을 추가하는 함수
void AddMemberFriendReq(UINT64 MainFriend, UINT64 TargetFriend);
// MemberReqFriendMap에서 특정 Member의 친구 요청 중 1개를 삭제하는 함수
void DeleteMemberFriendReq(UINT64 MainFriend, UINT64 TargetFriend);

// MemberResFriendMap에서 특정 Member의 친구 추가 요청리스트의 iterator를 받는 함수
std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> FindMemberFriendRes(UINT64 id);
// MemberResFriendMap에 특정 Member의 친구요청을 추가하는 함수
void AddMemberFriendRes(UINT64 MainFriend, UINT64 TargetFriend);
// MemberResFriendMap에서 특정 Member의 친구 요청 중 1개를 삭제하는 함수
void DeleteMemberFriendRes(UINT64 MainFriend, UINT64 TargetFriend);

// 유니캐스트 Send
void SendPacket_Unicast(st_SESSION* session, st_PACKET_HEADER* header, CMessage* message);

// 로그 파일 초기화 함수
void InitialLogFile();
// 로그파일에 에러입력하는 함수
void PrintError(const WCHAR* error);

// 서버가 꺼질 때 모든 세션을 정리하는 함수
void CloseAllSession();

int main()
{
	setlocale(LC_ALL, "");
	InitialLogFile();
	//LoadAllData();


	if (!InitialNetwork())
	{
		wprintf(L"네트워크 초기화 실패\n");
		return 0;
	}

	while (true)
	{

		NetWorkPart();
		
		if (GetAsyncKeyState(VK_SPACE) & 0x8000)
		{
			//SaveAllData();
			CloseAllSession();
			break;
		}
	}
	CloseAllSession();
	return 0;
}


bool InitialNetwork()
{
	int retval;
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		wprintf(L"WSAStartup() eror\n");
		return false;
	}

	// socket()
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == INVALID_SOCKET)
	{
		wprintf(L"socket() eror\n");
		return false;
	}

	// 넌블록 소켓으로 전환
	u_long on = 1;
	retval = ioctlsocket(listen_socket, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"ioctlsocket() eror\n");
		return false;
	}

	// 네이글 알고리즘 끄기
	BOOL optval = TRUE;
	setsockopt(listen_socket, IPPROTO_IP, TCP_NODELAY, (char*)&optval, sizeof(optval));

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(dfNETWORK_PORT);
	retval = bind(listen_socket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"bind() error\n");
		return false;
	}

	// listen()
	retval = listen(listen_socket, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"listen() error\n");
		return false;
	}

	return true;
}

void NetWorkPart(void)
{
	SESSION* pSession = nullptr;
	FD_SET readSet, writeSet;

	int addrlen, retval;
	int iSessionCount = 0;
	DWORD clientID[FD_SETSIZE];
	SOCKET clientSocket[FD_SETSIZE];
	memset(clientID, -1, FD_SETSIZE*sizeof(DWORD));
	memset(clientSocket, INVALID_SOCKET, FD_SETSIZE*sizeof(SOCKET));
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);

	 //리슨 소켓 넣기
	FD_SET(listen_socket, &readSet);
	clientID[iSessionCount] = 0;
	clientSocket[iSessionCount] = listen_socket;
	iSessionCount++;
	std::map<DWORD,SESSION*>::iterator itor;

	//DeleteClient();

	//wprintf(L"%d\n", (int)ClientList.size());

	for (itor = ClientList.begin(); itor != ClientList.end();)
	{
		pSession = (itor->second);
		itor++;
		if (pSession->socket == INVALID_SOCKET)
		{
			ClientList.erase(pSession->ID);
			delete pSession;
			continue;
		}
		clientID[iSessionCount] = pSession->ID;
		clientSocket[iSessionCount] = pSession->socket;

		FD_SET(pSession->socket, &readSet);
		if (pSession->SendQ.GetUsingSize() > 0)
		{
			FD_SET(pSession->socket, &writeSet);
		}
		iSessionCount++;
		
		if (iSessionCount >= FD_SETSIZE)
		{
			SelectSocket(clientID, clientSocket, &readSet, &writeSet);
			FD_ZERO(&readSet);
			FD_ZERO(&writeSet);
			memset(clientID, -1, FD_SETSIZE * sizeof(DWORD));
			memset(clientSocket, INVALID_SOCKET, FD_SETSIZE * sizeof(SOCKET));
			iSessionCount = 0;
		}
	}
	if (iSessionCount > 0)
	{
		SelectSocket(clientID, clientSocket, &readSet, &writeSet);
	}
}



CLIENT* FindMember(UINT64 id)
{
	CLIENT* temp = MemberMap.find(id)->second;
	return temp;
}

void AddMemeber(UINT64 id, CLIENT* pClient)
{
	MemberMap.insert(std::make_pair(id, pClient));
}

std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> FindMemberFriend(UINT64 id)
{
	std::pair< std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp;
	temp.first = MemberFriendMap.equal_range(id).first;
	temp.second = MemberFriendMap.equal_range(id).second;
	return temp;
}

void AddMemberFriend(UINT64 MainFriend, UINT64 TargetFriend)
{
	CLIENT* pMember = FindMember(TargetFriend);
	MemberFriendMap.insert(std::make_pair(MainFriend, pMember));
}

void DeleteMemberFriend(UINT64 MainFriend, UINT64 TargetFriend)
{
	typedef std::multimap<UINT64, CLIENT*>::iterator iter;
	std::pair<iter, iter> iterpair = MemberFriendMap.equal_range(MainFriend);
	iter it = iterpair.first;
	for (; it != iterpair.second; it++)
	{
		if (it->second->ID == TargetFriend)
		{
			MemberFriendMap.erase(it);
			break;
		}
	}
}

std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> FindMemberFriendReq(UINT64 id)
{
	std::pair< std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp;
	temp.first = MemberReqFriendMap.equal_range(id).first;
	temp.second = MemberReqFriendMap.equal_range(id).second;
	return temp;
}

void AddMemberFriendReq(UINT64 MainFriend, UINT64 TargetFriend)
{
	CLIENT* pMember = FindMember(TargetFriend);
	MemberReqFriendMap.insert(std::make_pair(MainFriend, FindMember(TargetFriend)));
	MemberResFriendMap.insert(std::make_pair(pMember->ID, FindMember(MainFriend)));
}

void DeleteMemberFriendReq(UINT64 MainFriend, UINT64 TargetFriend)
{
	typedef std::multimap<UINT64, CLIENT*>::iterator iter;
	// MainFriend에서 보낸 TargetFriend에게 보낸 친구 요청 삭제
	std::pair<iter, iter> iterpair = MemberReqFriendMap.equal_range(MainFriend);
	iter it = iterpair.first;
	for (; it != iterpair.second; it++)
	{
		if (it->second->ID == TargetFriend)
		{
			MemberFriendMap.erase(it);
			break;
		}
	}

	// TargetFriend가 MainFriend로부터 받은 친구 요청을 삭제
	std::pair<iter, iter> iterpair2 = MemberResFriendMap.equal_range(TargetFriend);
	iter it2 = iterpair2.first;
	for (; it2 != iterpair.second; it++)
	{
		if (it2->second->ID == MainFriend)
		{
			MemberResFriendMap.erase(it2);
			break;
		}
	}
}

std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> FindMemberFriendRes(UINT64 id)
{
	std::pair< std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp;
	temp.first = MemberResFriendMap.equal_range(id).first;
	temp.second = MemberResFriendMap.equal_range(id).second;
	return temp;
}

void AddMemberFriendRes(UINT64 MainFriend, UINT64 TargetFriend)
{
	CLIENT* pMember = FindMember(TargetFriend);
	MemberResFriendMap.insert(std::make_pair(MainFriend, FindMember(TargetFriend)));
	MemberReqFriendMap.insert(std::make_pair(pMember->ID, FindMember(MainFriend)));
}

void DeleteMemberFriendRes(UINT64 MainFriend, UINT64 TargetFriend)
{
	typedef std::multimap<UINT64, CLIENT*>::iterator iter;
	// MainFriend에서 보낸 TargetFriend에게 보낸 친구 요청 삭제
	std::pair<iter, iter> iterpair = MemberReqFriendMap.equal_range(MainFriend);
	iter it = iterpair.first;
	for (; it != iterpair.second; it++)
	{
		if (it->second->ID == TargetFriend)
		{
			MemberFriendMap.erase(it);
			break;
		}
	}

	// TargetFriend가 MainFriend로부터 받은 친구 요청을 삭제
	std::pair<iter, iter> iterpair2 = MemberResFriendMap.equal_range(TargetFriend);
	iter it2 = iterpair2.first;
	for (; it2 != iterpair.second; it++)
	{
		if (it2->second->ID == MainFriend)
		{
			MemberResFriendMap.erase(it2);
			break;
		}
	}
}

void SendPacket_Unicast(st_SESSION* session, st_PACKET_HEADER* header, CMessage* message)
{
	if (session == nullptr)
	{
		wprintf(L"SendPacket_Unicast Error!\n");
		return;
	}
	session->SendQ.Enqueue((char*)header, sizeof(st_PACKET_HEADER));
	session->SendQ.Enqueue((char*)message->GetBufferPtr(), message->GetDataSize());
}

void InitialLogFile()
{
	time_t now = time(NULL);
	struct tm date;
	setlocale(LC_ALL, "Korean");
	localtime_s(&date, &now);

	wcsftime(g_TextTitle, 64, L"FrienManage Server(Select Model)_%Y%m%d_%H%M%S.txt", &date);
	g_File = _wfopen(g_TextTitle, L"wb");

	fclose(g_File);
}

void PrintError(const WCHAR* error)
{
	time_t now = time(NULL);
	struct tm date;
	setlocale(LC_ALL, "Korean");
	localtime_s(&date, &now);
	WCHAR errortext[128] = { 0 };
	WCHAR time[64] = { 0 };

	wcscat(errortext, error);
	wcsftime(time, 64, L"_%Y%m%d_%H%M%S.txt\n", &date);

	wcscat(errortext, time);

	g_File = _wfopen(g_TextTitle, L"ab");

	fwprintf_s(g_File, L"%s", errortext);

	fclose(g_File);
}

void CloseAllSession()
{
	for (std::map<DWORD, SESSION*>::iterator itor = ClientList.begin(); itor != ClientList.end(); itor++)
	{
		closesocket((itor)->second->socket);
	}
	ClientList.clear();

	for (std::multimap<UINT64, CLIENT*>::iterator itor = MemberFriendMap.begin(); itor != MemberFriendMap.end(); itor++)
	{
		delete (itor->second);
	}
	MemberFriendMap.clear();
}

void SelectSocket(DWORD* dwTableNO,SOCKET* pTableSocket, FD_SET* pReadSet, FD_SET* pWriteSet)
{
	SESSION* pSession = nullptr;
	int iResult = 0;
	int iCnt = 0;

	iResult = select(0, pReadSet, pWriteSet, 0, &selecttime);

	if (iResult > 0)
	{
		for (int i = 0; i < FD_SETSIZE; i++)
		{
			if (pTableSocket[i] == INVALID_SOCKET)
				continue;

			if (FD_ISSET(pTableSocket[i], pReadSet))
			{
				// listen_sock이면 접속 처리
				if (dwTableNO[i] == 0)
					Accept();
				else
					NetWork_Recv(dwTableNO[i]);
			}

			if (FD_ISSET(pTableSocket[i], pWriteSet))
			{
				NetWork_Send(dwTableNO[i]);
			}
		}
	}
}

void Accept()
{
	SOCKADDR_IN clientaddr;
	SOCKET client_socket;
	int retval, addrlen;
	WCHAR szParam[16] = { 0 };
	SESSION* clientSessionPtr;

	addrlen = sizeof(clientaddr);
	client_socket = accept(listen_socket, (SOCKADDR*)&clientaddr, &addrlen);
	if (client_socket == INVALID_SOCKET)
	{
		PrintError(L"accept()");
		return;
	}

	clientSessionPtr = new SESSION;
	clientSessionPtr->socket = client_socket;
	clientSessionPtr->clientaddr = clientaddr;
	clientSessionPtr->ID = g_iSessionID++;
	InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
	//wprintf(L"\n[FriendManage 서버] 클라이언트 접속 : IP 주소 %s, 포트 번호 = %d, 세션 ID : %d\n", szParam, ntohs(clientaddr.sin_port), g_iSessionID - 1);
	AddSession(clientSessionPtr);
}

void NetWork_Recv(DWORD UserID)
{
	int iResult = 0;
	SESSION* pSession = nullptr;
	int iRecvBufferFreeSize = 0;
	int iEnqeueSize = 0;

	pSession = FindSession(UserID);
	if (pSession->socket == INVALID_SOCKET)
		return;

	// 서버를 꺼야하는 것 아닐까??
	if (pSession == nullptr)
		closesocket(listen_socket);

	iRecvBufferFreeSize = pSession->RecvQ.DirectEnqueueSize();
	iResult = recv(pSession->socket, pSession->RecvQ.GetRearBufferPtr(), iRecvBufferFreeSize, 0);

	if (iResult == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			DWORD temp = WSAGetLastError();
			wprintf(L"recv error : %d\n", temp);
			Disconnect(UserID);
			return;
		}

	}

	pSession->RecvQ.MoveRear(iResult);

	//if (iEnqeueSize != iResult)
	//{
	//	PrintError(L"FatalError EndQueue");
	//	exit(1);
	//}

	if (iResult > 0)
	{
		while (true)
		{
			iResult = CompletePacket(pSession);
			if (iResult == 1)
				break;
			// 패킷 처리 오류
			if (iResult == -1)
			{
				WCHAR error[64] = { 0 };
				swprintf_s(error, 64, L"Packet Error UserID : %d\n", pSession->ID);
				PrintError(error);
				return;
			}
		}
	}

}

void Disconnect(DWORD UserID)
{
	WCHAR szParam[16] = { 0 };
	SESSION* pSession = FindSession(UserID);

	InetNtop(AF_INET, &(pSession->clientaddr.sin_addr), szParam, 16);

	wprintf(L"\n[Select 서버] 클라이언트 종료 : IP 주소 %s, 포트 번호 = %d\n", szParam, ntohs(pSession->clientaddr.sin_port));
	closesocket(pSession->socket);
	pSession->socket = INVALID_SOCKET;
}

void DeleteClient()
{
	std::map<DWORD, SESSION*>::iterator itor;
	for (itor = ClientList.begin(); itor != ClientList.end();)
	{
		if ((itor)->second->socket == INVALID_SOCKET)
		{
			wprintf(L"Client Delete ID %d\n", itor->first);
			free((itor)->second);
			ClientList.erase(itor++);
		}
		else
		{
			itor++;
		}
	}
}

int CompletePacket(SESSION* session)
{
	st_PACKET_HEADER PacketHeader;
	int iRecvQSize = session->RecvQ.GetUsingSize();

	if (iRecvQSize < sizeof(PacketHeader))
		return 1;

	// 패킷 검사
	session->RecvQ.Peek((char*)&PacketHeader, sizeof(st_PACKET_HEADER));

	// 패킷 코드 검사
	if (PacketHeader.byCode != dfPACKET_CODE)
		return -1;

	if (PacketHeader.wPayloadSize + sizeof(st_PACKET_HEADER) > iRecvQSize)
		return 1;

	session->RecvQ.MoveFront(sizeof(st_PACKET_HEADER));

	CMessage Packet;
	if (PacketHeader.wPayloadSize != session->RecvQ.Dequeue(Packet.GetBufferPtr(), PacketHeader.wPayloadSize))
		return -1;

	Packet.MoveWritePos(PacketHeader.wPayloadSize);

	if (!PacketProc(session, PacketHeader.wMsgType, &Packet))
	{
		wprintf(L"PacketProc Error!\n");
		return -1;
	}
	return 0;
}

void NetWork_Send(DWORD UserID)
{
	SESSION* pSession = nullptr;
	pSession = FindSession(UserID);
	int iResult = 0;
	int iSendBufferUsingSize = 0;
	int iDequeueSize = 0;
	if (pSession->socket == INVALID_SOCKET)
		return;
	while (true)
	{
		if (pSession->SendQ.GetUsingSize() <= 0)
			break;

		if (pSession == nullptr)
			closesocket(listen_socket);

		iSendBufferUsingSize = pSession->SendQ.DirectDequeueSize();

		if (iSendBufferUsingSize <= 0)
			return;

		iResult = send(pSession->socket, pSession->SendQ.GetFrontBufferPtr(), iSendBufferUsingSize, 0);


		if (iResult == SOCKET_ERROR || iResult == 0)
		{
			DWORD error = WSAGetLastError();
			if (WSAEWOULDBLOCK == error)
			{
				wprintf(L"Socket WOULDBLOCK - UerID : %d\n", UserID);
				return;
			}
			wprintf(L"Socket Error - Error : %d, UserID : %d\n", error, UserID);
			Disconnect(UserID);
			return;
		}

		pSession->SendQ.MoveFront(iResult);

	}

	return;
}

bool PacketProc(SESSION* session, WORD dwType, CMessage* message)
{
	//wprintf(L"Packet Info UserID : %d, Message Type : %d\n", session->ID, dwType);
	switch (dwType)
	{
	case df_REQ_ACCOUNT_ADD:
		return NetWork_ReqRegister(session, message);
		break;
	case df_REQ_LOGIN:
		return NetWork_ReqLogin(session, message);
		break;
	case df_REQ_ACCOUNT_LIST:
		return NetWork_ReqAccountList(session);
		break;
	case df_REQ_FRIEND_LIST:
		return NetWork_ReqFriendList(session);
		break;
	case df_REQ_FRIEND_REQUEST_LIST:
		return NetWork_ReqFriendReqList(session);
		break;
	case df_REQ_FRIEND_REPLY_LIST:
		return NetWork_ReqFriendResList(session);
	case df_REQ_FRIEND_REMOVE:
		return NetWork_ReqFriendRemove(session, message);
		break;
	case df_REQ_FRIEND_REQUEST:
		return NetWork_ReqFriendReq(session, message);
		break;
	case df_REQ_FRIEND_CANCEL:
		return NetWork_ReqFriendRedRemove(session, message);
		break;
	case df_REQ_FRIEND_DENY:
		return NetWork_ReqFriendReqDeny(session, message);
		break;
	case df_REQ_FRIEND_AGREE:
		return NetWork_ReqFriendReqAgree(session, message);
		break;
	case df_REQ_STRESS_ECHO:
		return NetWork_ReqEcho(session, message);
		break;
	default:
		break;
	}
	return false;
}

void SaveAllData(void)
{
	FILE* pFile = nullptr;
	pFile = fopen("Json.txt", "wb");
	if (pFile == nullptr)
	{
		wprintf(L"File open fail\n");
		return;
	}

	rapidjson::StringBuffer StringJson;
	rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF16<>> writer(StringJson);
	//rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>, rapidjson::UTF16<>> writer(temp);

	writer.StartObject();
	writer.String(L"Member");
	writer.StartArray();

	for (std::unordered_multimap< UINT64, CLIENT*>::iterator itor = MemberMap.begin(); itor != MemberMap.end(); itor++)
	{
		writer.StartObject();
		writer.String(L"AccountNO");
		writer.Uint64(itor->first);
		writer.String(L"NickName");
		writer.String(itor->second->NickName);
		writer.EndObject();
	}
	writer.EndArray();
	writer.EndObject();

	g_JsonSize = StringJson.GetSize();
	const char* pJson = StringJson.GetString();

	fwrite(pJson, g_JsonSize, 1, pFile);
	fclose(pFile);
}

void LoadAllData(void)
{
	FILE* pFile = fopen("Json.txt", "rb");
	if (pFile == nullptr)
	{
		wprintf(L"File load failed\n");
		return;
	}
	fseek(pFile, 0, SEEK_END);
	int lSize = ftell(pFile);
	rewind(pFile);

	char* buffer = (char*)malloc(sizeof(char) * lSize + 1);

	int result = fread(buffer, 1, lSize, pFile);
	if (result != lSize)
		return;

	buffer[lSize] = 0;

	rapidjson::Document Doc;
	Doc.Parse(buffer);
	if (Doc.HasParseError())
		return;

	UINT64 AccountNo;

	rapidjson::Value& AccountArray = Doc["Member"];
	WCHAR szNickName[dfNICK_MAX_LEN];
	for (rapidjson::SizeType i = 0; i < AccountArray.Size(); i++)
	{
		rapidjson::Value& AccountObejct = AccountArray[i];
		AccountNo = AccountObejct["AccountNO"].GetUint64();
		int len = MultiByteToWideChar(CP_UTF8, 0, AccountObejct["NickName"].GetString(), strlen(AccountObejct["NickName"].GetString()), szNickName, 100);
		szNickName[len] = L'\0';
		CLIENT* newClient = new CLIENT();
		newClient->ID = AccountNo;
		memcpy(newClient->NickName, szNickName, dfNICK_MAX_LEN);
		MemberMap.insert({ AccountNo,  newClient });
	}

	fclose(pFile);
	free(buffer);
	return;
}

bool NetWork_ReqRegister(SESSION* session, CMessage* message)
{
	CLIENT* pClient = new CLIENT();
	WCHAR temp[dfNICK_MAX_LEN] = { '\0' };
	memcpy(pClient->NickName, message->GetBufferPtr(), sizeof(temp));
	pClient->NickName[19] = '\0';
	pClient->ID = g_iClientID++;
	AddMemeber(pClient->ID, pClient);
	NetWork_ResRegister(session, df_RES_ACCOUNT_ADD, pClient->ID);
	return true;
}

bool NetWork_ReqLogin(SESSION* session, CMessage* message)
{
	UINT64 ReqClientID;
	(*message) >> ReqClientID;
	CLIENT* pSession = FindMember(ReqClientID);
	if (pSession == nullptr)
		NetWork_ResLogin(session, nullptr);
	else
	{
		NetWork_ResLogin(session, pSession);
		g_iLoginClientID = pSession->ID;
	}
	return true;
}

bool NetWork_ReqAccountList(SESSION* session)
{
	NetWork_ResAccountList(session);
	return true;
}

bool NetWork_ReqFriendList(SESSION* session)
{
	NetWork_ResFriendList(session);
	return true;
}

bool NetWork_ReqFriendReqList(SESSION* session)
{
	NetWork_ResFriendReqList(session);
	return true;
}

bool NetWork_ReqFriendResList(SESSION* session)
{
	NetWork_ResFriendResList(session);
	return true;
}

bool NetWork_ReqFriendRemove(SESSION* session, CMessage* message)
{
	UINT64 TargetClientID;
	(*message) >> TargetClientID;
	if (g_iLoginClientID == 0)
	{
		NetWork_ResFriendRemove(session, df_RESULT_FRIEND_REMOVE_FAIL, TargetClientID);
		return true;
	}
	std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp = FindMemberFriend(g_iLoginClientID);
	BOOL Flag = FALSE;
	for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
	{
		if (itor->first == TargetClientID)
		{
			Flag = TRUE;
			MemberFriendMap.erase(itor);
			break;
		}
	}
	if (Flag == FALSE)
	{
		NetWork_ResFriendRemove(session, df_RESULT_FRIEND_REMOVE_NOTFRIEND, TargetClientID);
		return true;
	}
	else
	{
		NetWork_ResFriendRemove(session, df_RESULT_FRIEND_REMOVE_OK, TargetClientID);
		return true;
	}
}

bool NetWork_ReqFriendReq(SESSION* session, CMessage* message)
{
	UINT64 TargetClientID;
	(*message) >> TargetClientID;
	if (g_iLoginClientID == 0)
	{
		NetWork_ResFriendReq(session, df_RESULT_FRIEND_REQUEST_NOTFOUND, TargetClientID);
		return true;
	}
	CLIENT* targetClient = FindMember(TargetClientID);
	if (targetClient == nullptr)
	{
		NetWork_ResFriendReq(session, df_RESULT_FRIEND_REQUEST_NOTFOUND, TargetClientID);
		return true;
	}
	else
	{
		std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp = FindMemberFriendReq(g_iLoginClientID);
		BOOL Flag = false;
		for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
		{
			if (itor->second->ID == TargetClientID)
			{
				Flag = TRUE;
				break;
			}
		}
		if (Flag)
		{
			NetWork_ResFriendReq(session, df_RESULT_FRIEND_REQUEST_AREADY, TargetClientID);
			return true;
		}
		else
		{
			MemberReqFriendMap.insert(std::make_pair(g_iLoginClientID, FindMember(TargetClientID)));
			MemberResFriendMap.insert(std::make_pair(TargetClientID, FindMember(g_iLoginClientID)));
			NetWork_ResFriendReq(session, df_RESULT_FRIEND_REQUEST_OK, TargetClientID);
		}
	}
	return true;
}

bool NetWork_ReqFriendRedRemove(SESSION* session, CMessage* message)
{
	UINT64 TargetClientID;
	(*message) >> TargetClientID;
	if (g_iLoginClientID == 0)
	{
		NetWork_ResFriendReqRemove(session, df_RESULT_FRIEND_CANCEL_FAIL, TargetClientID);
		return true;
	}
	CLIENT* targetClient = FindMember(TargetClientID);
	if (targetClient == nullptr)
	{
		NetWork_ResFriendReqRemove(session, df_RESULT_FRIEND_CANCEL_FAIL, TargetClientID);
		return true;
	}
	else
	{
		std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp = FindMemberFriendReq(g_iLoginClientID);
		std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp1 = FindMemberFriendRes(TargetClientID);
		BOOL Flag = false;
		for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
		{
			if (itor->second->ID == TargetClientID)
			{
				MemberReqFriendMap.erase(itor);
				for (std::multimap<UINT64, CLIENT*>::iterator itor1 = temp1.first; itor1 != temp1.second; itor1++)
				{
					if (itor1->second->ID == g_iLoginClientID)
					{
						MemberResFriendMap.erase(itor1);
						break;
					}
				}
				Flag = TRUE;
				break;
			}
		}
		if (Flag == TRUE)
		{
			NetWork_ResFriendReqRemove(session, df_RESULT_FRIEND_CANCEL_OK, TargetClientID);
			return true;
		}
		else
		{
			NetWork_ResFriendReqRemove(session, df_RESULT_FRIEND_CANCEL_NOTFRIEND, TargetClientID);
			return true;
		}
	}
}

bool NetWork_ReqFriendReqDeny(SESSION* session, CMessage* message)
{
	UINT64 TargetClientID;
	(*message) >> TargetClientID;
	if (g_iLoginClientID == 0)
	{
		NetWork_ResFriendReqDeny(session, df_RESULT_FRIEND_DENY_FAIL, TargetClientID);
		return true;
	}
	CLIENT* temp = FindMember(TargetClientID);
	if (temp == nullptr)
	{
		NetWork_ResFriendReqDeny(session, df_RESULT_FRIEND_DENY_FAIL, TargetClientID);
		return true;
	}
	else
	{
		std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp = FindMemberFriendReq(g_iLoginClientID);
		BOOL Flag = false;
		for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
		{
			if (itor->second->ID == TargetClientID)
			{
				MemberReqFriendMap.erase(itor);
				Flag = TRUE;
				break;
			}
		}
		if (Flag == TRUE)
		{
			std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp2 = FindMemberFriendRes(TargetClientID);
			for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
			{
				if (itor->second->ID == g_iLoginClientID)
				{
					MemberReqFriendMap.erase(itor);
					break;
				}
			}
			NetWork_ResFriendReqDeny(session, df_RESULT_FRIEND_DENY_OK, TargetClientID);
			return true;
		}
		else
		{
			NetWork_ResFriendReqDeny(session, df_RESULT_FRIEND_DENY_NOTFRIEND, TargetClientID);
			return true;
		}
	}
}

bool NetWork_ReqFriendReqAgree(SESSION* session, CMessage* message)
{
	UINT64 TargetClientID;
	(*message) >> TargetClientID;
	if (g_iLoginClientID == 0)
	{
		NetWork_ResFriendReqAgree(session, df_RESULT_FRIEND_AGREE_FAIL, TargetClientID);
		return true;
	}
	CLIENT* pTargetClient = FindMember(TargetClientID);
	if (pTargetClient == nullptr)
	{
		NetWork_ResFriendReqAgree(session, df_RESULT_FRIEND_AGREE_FAIL, TargetClientID);
		return true;
	}
	else
	{
		std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp = FindMemberFriendRes(g_iLoginClientID);
		BOOL Flag = false;
		for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
		{
			if (itor->second->ID == TargetClientID)
			{
				MemberReqFriendMap.erase(itor);
				Flag = TRUE;
				break;
			}
		}
		if (Flag == TRUE)
		{
			std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp2 = FindMemberFriendReq(TargetClientID);
			for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
			{
				if (itor->second->ID == g_iLoginClientID)
				{
					MemberReqFriendMap.erase(itor);
					break;
				}
			}
			MemberFriendMap.insert(std::make_pair(g_iLoginClientID, pTargetClient));
			MemberFriendMap.insert(std::make_pair(TargetClientID, FindMember(g_iLoginClientID)));
			NetWork_ResFriendReqAgree(session, df_RESULT_FRIEND_AGREE_OK, TargetClientID);
			return true;
		}
		else
		{
			NetWork_ResFriendReqAgree(session, df_RESULT_FRIEND_DENY_NOTFRIEND, TargetClientID);
			return true;
		}
	}
}

bool NetWork_ReqEcho(SESSION* session, CMessage* message)
{
	NetWork_ResEcho(session, message);
	return true;
}

void NetWork_ResRegister(SESSION* session, WORD byResult, UINT64 pClientID)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResRegister(&header, &packet, byResult, pClientID);

	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResLogin(SESSION* session, CLIENT* pSession)
{
	CMessage packet;
	st_PACKET_HEADER header;

	MakePacket_ResLogin(&header, &packet, pSession);

	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResAccountList(SESSION* session)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResAccountList(&header, &packet);
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResFriendList(SESSION* session)
{
	CMessage packet;
	st_PACKET_HEADER header;
	if (g_iLoginClientID <= 0)
	{
		MakePacket_ResFriendList(&header, &packet, FALSE);
	}
	else
	{
		MakePacket_ResFriendList(&header, &packet, TRUE);
	}
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResFriendReqList(SESSION* session)
{
	CMessage packet;
	st_PACKET_HEADER header;
	if (g_iLoginClientID <= 0)
	{
		MakePacket_ResFriendReqList(&header, &packet, FALSE);
	}
	else
	{
		MakePacket_ResFriendReqList(&header, &packet, TRUE);
	}
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResFriendResList(SESSION* session)
{
	CMessage packet;
	st_PACKET_HEADER header;
	if (g_iLoginClientID <= 0)
	{
		MakePacket_ResFriendResList(&header, &packet, FALSE);
	}
	else
	{
		MakePacket_ResFriendResList(&header, &packet, TRUE);
	}
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResFriendRemove(SESSION* session, BYTE result, UINT64 ClientID)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResFriendRemove(&header, &packet, result, ClientID);
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResFriendReq(SESSION* session, BYTE result, UINT64 ClientID)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResFriendReq(&header, &packet, result, ClientID);
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResFriendReqRemove(SESSION* session, BYTE result, UINT64 ClientID)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResFriendReqRemove(&header, &packet, result, ClientID);
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResFriendReqDeny(SESSION* session, BYTE result, UINT64 ClientID)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResFriendReqDeny(&header, &packet, result, ClientID);
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResFriendReqAgree(SESSION* session, BYTE result, UINT64 ClientID)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResFriendReqAgree(&header, &packet, result, ClientID);
	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResEcho(SESSION* session, CMessage* message)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResEcho(&header, &packet, message);
	SendPacket_Unicast(session, &header, &packet);
}

void MakePacket_ResRegister(st_PACKET_HEADER* pHeader, CMessage* message, WORD result, UINT64 ID)
{
	(*message) << (long long)ID;
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_ACCOUNT_ADD;
	pHeader->wPayloadSize = (WORD)sizeof(UINT64);
}

void MakePacket_ResLogin(st_PACKET_HEADER* pHeader, CMessage* message, CLIENT* pSession)
{
	message->Clear();
	if (pSession == nullptr)
	{
		(*message) << (long long)0;
	}
	else
	{
		UINT64 ID = pSession->ID;
		(*message) << (long long)ID;
		message->PutData((char*)pSession->NickName, 40);
	}
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_LOGIN;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResAccountList(st_PACKET_HEADER* pHeader, CMessage* message)
{
	UINT size = MemberMap.size();
	(*message) << (int)size;
	for (std::unordered_map<UINT64, CLIENT*>::iterator itor = MemberMap.begin(); itor != MemberMap.end(); itor++)
	{
		UINT64 id = itor->first;
		(*message) << (long long)id;
		message->PutData((char*)itor->second->NickName, sizeof(WCHAR) * dfNICK_MAX_LEN);
	}
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_ACCOUNT_LIST;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResFriendList(st_PACKET_HEADER* pHeader, CMessage* message, BOOL isLogined)
{
	if (isLogined == TRUE)
	{
		std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp = FindMemberFriend(g_iLoginClientID);
		(*message) << (int)MemberFriendMap.count(g_iLoginClientID);
		for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
		{
			(*message) << (long long)itor->first;
			message->PutData((char*)itor->second->NickName, 40);
		}
	}
	else
	{
		//(*message) << (int)0;
	}
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_LIST;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResFriendReqList(st_PACKET_HEADER* pHeader, CMessage* message, BOOL isLogined)
{
	if (isLogined == TRUE)
	{
		std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp = FindMemberFriendReq(g_iLoginClientID);
		(*message) << (int)MemberReqFriendMap.count(g_iLoginClientID);
		for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
		{
			(*message) << (long long)itor->first;
			message->PutData((char*)itor->second->NickName, 40);
		}
	}
	else
	{
		//(*message) << (int)0;
	}
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_REQUEST_LIST;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResFriendResList(st_PACKET_HEADER* pHeader, CMessage* message, BOOL isLogined)
{
	if (isLogined == TRUE)
	{
		std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> temp = FindMemberFriendRes(g_iLoginClientID);
		(*message) << (int)MemberResFriendMap.count(g_iLoginClientID);
		for (std::multimap<UINT64, CLIENT*>::iterator itor = temp.first; itor != temp.second; itor++)
		{
			(*message) << (long long)itor->first;
			message->PutData((char*)itor->second->NickName, 40);
		}
	}
	else
	{
		//(*message) << (int)0;
	}
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_REPLY_LIST;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResFriendRemove(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID)
{
	(*message) << (double)ClientID;
	(*message) << result;
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_REMOVE;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResFriendReq(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID)
{
	(*message) << (double)ClientID;
	(*message) << result;
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_REQUEST;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResFriendReqRemove(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID)
{
	(*message) << (double)ClientID;
	(*message) << result;
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_CANCEL;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResFriendReqDeny(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID)
{
	(*message) << (double)ClientID;
	(*message) << result;
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_DENY;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResFriendReqAgree(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, UINT64 ClientID)
{
	(*message) << (double)ClientID;
	(*message) << result;
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_AGREE;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResEcho(st_PACKET_HEADER* pHeader, CMessage* message, CMessage* text)
{
	WORD size = 0;
	(*text) >> size;
	(*message) << size;
	message->PutData((char*)text->GetBufferPtr() + 2, size);
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_STRESS_ECHO;
	pHeader->wPayloadSize = message->GetDataSize();
}

SESSION* FindSession(DWORD id)
{
	SESSION* pSession = nullptr;

	pSession = ClientList.find(id)->second;

	return pSession;
}

void AddSession(SESSION* session)
{
	ClientList.insert(std::make_pair(session->ID, session));
}
