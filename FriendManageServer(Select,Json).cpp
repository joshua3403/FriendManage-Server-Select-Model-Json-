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

// ��������
SOCKET listen_socket = INVALID_SOCKET;
// select���� ���ð�
timeval selecttime = { 0,0 };
// ���� �α׸� ����� ���� ������
FILE* g_File;
// ���� �α� ������ ����
WCHAR g_TextTitle[64] = { 0 };
// ���� ID
int g_iSessionID = 1;
// Ŭ���̾�Ʈ ID
int g_iClientID = 1;
// �α����� Ŭ���̾�Ʈ�� ID
int g_iLoginClientID = 0;

// ������ ���ӵ� ��� client�� �����ϴ� Map
std::list<SESSION*> ClientList;
// ȸ�����Ե� ȸ���� �����ϴ� Map
std::unordered_map<DWORD, CLIENT*> MemberMap;
// ���Ե� ȸ������ ģ������Ʈ�� �����ϴ� multimap
std::multimap<UINT64, CLIENT*> MemberFriendMap;
// Ư�� ȸ���� Ư��ȸ������ ���� ģ����û ����Ʈ�� �����ϴ� multimap, first = ��û�� ȸ���� ID, second = ��û���� ȸ���� CLIENT*
std::multimap<UINT64, CLIENT*> MemberReqFriendMap;
// Ư�� ȸ���� Ư��ȸ������ ���� ģ����û ����Ʈ�� �����ϴ� multimap, first = ��û ���� ȸ���� ID, second = ��û�� ȸ���� CLIENT*
std::multimap<UINT64, CLIENT*> MemberResFriendMap;

// ��Ʈ��ũ �ʱ�ȭ
bool InitialNetwork(void);
// ��Ʈ��ũ ������ ���ư��� �Լ�(select)
void NetWorkPart(void);
// ��Ʈ��ũ ���� �ȿ��� ���������� select�Լ��� ȣ���ϴ� �Լ�
void SelectSocket(std::vector<int> dwTableNO, std::vector<SOCKET> pTableSocket, FD_SET* pReadSet, FD_SET* pWriteSet);
// Ŭ���̾�Ʈ�� connect() ��û�� accpet()�ϴ� �Լ�
void Accept();
// �ش� ���Ͽ� recv�ϴ� �Լ�
void NetWork_Recv(DWORD UserID);
// ���� ���� ����
void Disconnect(DWORD UserID);
// ���������� ClientList�Լ����� session�� �����ϴ� �Լ�
void DeleteClient();
// ��Ŷ ������ ó�� ����
int CompletePacket(SESSION* session);
// �ش� ������ send�ϴ� �Լ�
void NetWork_Send(DWORD UserID);
// �������� ��Ŷ ó�� �Լ�
bool PacketProc(SESSION* session, WORD dwType, CMessage* message);

// ��Ŷ ���� �Լ�
// Ŭ���̾�Ʈ�� ��û
bool NetWork_ReqRegister(SESSION* session, CMessage* message);
bool NetWork_ReqLogin(SESSION* session, CMessage* message);
bool NetWork_ReqAccountList(SESSION* session);
bool NetWork_ReqFriendList(SESSION* session);

// ������ ����
void NetWork_ResRegister(SESSION* session, WORD Result, UINT64 pClientID);
void NetWork_ResLogin(SESSION* session, CLIENT* pSession);
void NetWork_ResAccountList(SESSION* session);
void NetWork_ResFriendList(SESSION* session);

// ��Ŷ ����� �Լ�
void MakePacket_ResRegister(st_PACKET_HEADER* pHeader, CMessage* message, WORD result, UINT64 ID);
void MakePacket_ResLogin(st_PACKET_HEADER* pHeader, CMessage* message, CLIENT* pSession);
void MakePacket_ResAccountList(st_PACKET_HEADER* pHeader, CMessage* message);
void MakePacket_ResFriendList(st_PACKET_HEADER* pHeader, CMessage* message, BOOL isLogined);

// ClientList���� ID�� key�� SESSION*�� ��ȯ�ϴ� �Լ�
SESSION* FindSession(DWORD id);
// ClientList�� ���ο� Client�� �����ϴ� �Լ�
void AddSession(SESSION* session);
//// ClientList���� �����ϴ� �Լ�
//SESSION* DeleteSession(DWORD id);

// MemberMap���� ȸ���� �г����� ã���ִ� �Լ�
CLIENT* FindMember(UINT64 id);
// ���ο� ȸ���� �߰��ϴ� �Լ�
void AddMemeber(UINT64 id, WCHAR* nickName);
// MemberMap���� ȸ���� �����ϴ� �Լ�
void DeleteMember(UINT64 id);

// MemberFriendMap���� Ư�� Member�� ģ�� iterator�� �޴� �Լ�
std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> FindMemberFriend(UINT64 id);
// MemberFriendMap�� Ư�� Member�� ģ���� �߰��ϴ� �Լ�
void AddMemberFriend(UINT64 MainFriend, UINT64 TargetFriend);
// MemberFriendMap���� Ư�� Member�� ģ�� �� 1���� �����ϴ� �Լ�
void DeleteMemberFriend(UINT64 MainFriend, UINT64 TargetFriend);

// MemberReqFriendMap���� Ư�� Member�� ģ�� �߰� ��û����Ʈ�� iterator�� �޴� �Լ�
std::pair<std::multimap<UINT64, CLIENT*>::iterator, std::multimap<UINT64, CLIENT*>::iterator> FindMemberFriendReq(UINT64 id);
// MemberReqFriendMap�� Ư�� Member�� ģ����û�� �߰��ϴ� �Լ�
void AddMemberFriendReq(UINT64 MainFriend, UINT64 TargetFriend);
// MemberReqFriendMap���� Ư�� Member�� ģ�� ��û �� 1���� �����ϴ� �Լ�
void DeleteMemberFriendReq(UINT64 MainFriend, UINT64 TargetFriend);

// ����ĳ��Ʈ Send
void SendPacket_Unicast(st_SESSION* session, st_PACKET_HEADER* header, CMessage* message);

// �α� ���� �ʱ�ȭ �Լ�
void InitialLogFile();
// �α����Ͽ� �����Է��ϴ� �Լ�
void PrintError(const WCHAR* error);

// ������ ���� �� ��� ������ �����ϴ� �Լ�
void CloseAllSession();

int main()
{
	setlocale(LC_ALL, "");
	InitialLogFile();

	if (!InitialNetwork())
	{
		wprintf(L"��Ʈ��ũ �ʱ�ȭ ����\n");
		return 0;
	}

	while (true)
	{
		NetWorkPart();
	}

	CloseAllSession();
	return 0;
}


bool InitialNetwork()
{
	int retval;
	// ���� �ʱ�ȭ
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

	// �ͺ�� �������� ��ȯ
	u_long on = 1;
	retval = ioctlsocket(listen_socket, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"ioctlsocket() eror\n");
		return false;
	}

	// ���̱� �˰��� ����
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
	std::vector<int> clientID(FD_SETSIZE);
	std::vector<SOCKET> clientSocket(FD_SETSIZE);
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);

	// ���� ���� �ֱ�
	FD_SET(listen_socket, &readSet);
	clientID[iSessionCount] = 0;
	clientSocket[iSessionCount] = listen_socket;
	iSessionCount++;
	std::list<SESSION*>::iterator itor;

	DeleteClient();

	for (itor = ClientList.begin(); itor != ClientList.end();)
	{
		pSession = (*itor);
		itor++;

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
			clientID.assign(64, 0);
			clientSocket.assign(64, INVALID_SOCKET);
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

void AddMemeber(DWORD id, CLIENT* pClient)
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
	for (std::list<SESSION*>::iterator itor = ClientList.begin(); itor != ClientList.end(); itor++)
	{
		closesocket((*itor)->socket);
		delete (*itor);
	}
	ClientList.clear();

	for (std::multimap<UINT64, CLIENT*>::iterator itor = MemberFriendMap.begin(); itor != MemberFriendMap.end(); itor++)
	{
		delete (itor->second);
	}
	MemberFriendMap.clear();
}

void SelectSocket(std::vector<int> dwTableNO, std::vector<SOCKET> pTableSocket, FD_SET* pReadSet, FD_SET* pWriteSet)
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
				// listen_sock�̸� ���� ó��
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
	wprintf(L"\n[FriendManage ����] Ŭ���̾�Ʈ ���� : IP �ּ� %s, ��Ʈ ��ȣ = %d\n", szParam, ntohs(clientaddr.sin_port));
	AddSession(clientSessionPtr);
}

void NetWork_Recv(DWORD UserID)
{
	int iResult = 0;
	SESSION* pSession = nullptr;
	char buffer[BUFSIZE] = { 0 };
	int iRecvBufferFreeSize = 0;
	int iEnqueueSize = 0;

	pSession = FindSession(UserID);

	// ������ �����ϴ� �� �ƴұ�??
	if (pSession == nullptr)
		closesocket(listen_socket);

	iRecvBufferFreeSize = pSession->RecvQ.GetFreeSize();
	char* temp = pSession->RecvQ.GetRearBufferPtr();
	iResult = recv(pSession->socket, temp, iRecvBufferFreeSize, 0);

	pSession->RecvQ.MoveRear(iResult);


	if (iResult == SOCKET_ERROR || iResult == 0)
	{
		Disconnect(UserID);
		wprintf(L"%d\n", (int)ClientList.size());
		return;
	}


	if (iResult > 0)
	{
		while (true)
		{
			iResult = CompletePacket(pSession);
			if (iResult == 1)
				break;
			// ��Ŷ ó�� ����
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

	wprintf(L"\n[Select ����] Ŭ���̾�Ʈ ���� : IP �ּ� %s, ��Ʈ ��ȣ = %d\n", szParam, ntohs(pSession->clientaddr.sin_port));
	closesocket(pSession->socket);
}

void DeleteClient()
{
	std::list<SESSION*>::iterator itor;
	for (itor = ClientList.begin(); itor != ClientList.end();)
	{
		if ((*itor)->socket == INVALID_SOCKET)
		{
			int temp = (*itor)->ID;
			free((*itor));
			itor = ClientList.erase(itor);
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

	// ��Ŷ �˻�
	session->RecvQ.Peek((char*)&PacketHeader, sizeof(st_PACKET_HEADER));

	// ��Ŷ �ڵ� �˻�
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
	int iResult = 0;
	SESSION* pSession = nullptr;
	char buffer[BUFSIZE] = { 0 };
	int iSendBufferUsingSize = 0;
	int iDequeueSize = 0;

	pSession = FindSession(UserID);
	if (pSession == nullptr)
		closesocket(listen_socket);

	iSendBufferUsingSize = pSession->SendQ.GetUsingSize();

	if (iSendBufferUsingSize <= 0)
		return;

	iResult = send(pSession->socket, pSession->SendQ.GetFrontBufferPtr(), iSendBufferUsingSize, 0);
	pSession->SendQ.MoveFront(iResult);

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

	return;
}

bool PacketProc(SESSION* session, WORD dwType, CMessage* message)
{
	wprintf(L"Packet Info UserID : %d, Message Type : %d\n", session->ID, dwType);
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
	default:
		break;
	}
	return false;
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
	for (std::unordered_map<DWORD, CLIENT*>::iterator itor = MemberMap.begin(); itor != MemberMap.end(); itor++)
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
			message->PutData((char*)itor->second, 40);
		}
	}
	else
	{
		(*message) << (int)0;
	}
	pHeader->byCode = dfPACKET_CODE;
	pHeader->wMsgType = df_RES_FRIEND_LIST;
	pHeader->wPayloadSize = message->GetDataSize();
}

SESSION* FindSession(DWORD id)
{
	SESSION* pSession = nullptr;

	for (std::list<SESSION*>::iterator itor = ClientList.begin(); itor != ClientList.end(); itor++)
	{
		if ((*itor)->ID == id)
		{
			pSession = (*itor);
			break;
		}
	}

	return pSession;
}

void AddSession(SESSION* session)
{
	ClientList.push_back(session);
}
