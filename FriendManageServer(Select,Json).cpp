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

// 서버에 접속된 모든 client를 관리하는 Map
std::unordered_map<int, SESSION*> clientMap;

// 회원가입된 회원을 관리하는 Map
std::unordered_map<int, WCHAR*> MemberMap;

// clientMap에서 ID를 key로 SESSION*를 반환하는 함수
SESSION* FindSession(int id);
// clientMap에 새로운 Client를 삽입하는 함수
void AddSession(SESSION* session);
// clientMap에서 삭제하는 함수
void DeleteSession(int id);

// MemberMap에서 회원의 닉네임을 찾아주는 함수
WCHAR* FindMember(int id);
// 새로운 회원을 추가하는 함수
void AddMemeber(int id, WCHAR* nickName);
// MemberMap에서 회원을 삭제하는 함수
void DeleteMember(int id);