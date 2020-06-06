#include "stdafx.h"
#include "Protocol.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

typedef struct st_SESSION
{
	SOCKET socket;
	SOCKADDR_IN clientaddr;
	int ID;
	WCHAR NickName[dfNICK_MAX_LEN];
	BOOL bIsLogin;

	st_SESSION(void)
	{
		socket = INVALID_SOCKET;
		ZeroMemory(&clientaddr, sizeof(clientaddr));
		ID = 0;
		NickName[0] = { '\0' };
		bIsLogin = FALSE;
	}
	~st_SESSION(void) {}
} SESSION;

// ������ ���ӵ� ��� client�� �����ϴ� Map
std::unordered_map<int, SESSION*> clientMap;

// ȸ�����Ե� ȸ���� �����ϴ� Map
std::unordered_map<int, WCHAR*> MemberMap;

// clientMap���� ID�� key�� SESSION*�� ��ȯ�ϴ� �Լ�
SESSION* FindSession(int id);
// clientMap�� ���ο� Client�� �����ϴ� �Լ�
void AddSession(SESSION* session);
// clientMap���� �����ϴ� �Լ�
void DeleteSession(int id);

// MemberMap���� ȸ���� �г����� ã���ִ� �Լ�
WCHAR* FindMember(int id);
// ���ο� ȸ���� �߰��ϴ� �Լ�
void AddMemeber(int id, WCHAR* nickName);
// MemberMap���� ȸ���� �����ϴ� �Լ�
void DeleteMember(int id);