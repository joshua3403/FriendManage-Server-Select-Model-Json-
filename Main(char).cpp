#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <utility>
#include <string>
#include <map>
#include <time.h>
#include <windows.h>
#include <wchar.h>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

int main()
{
	std::map<int, std::wstring> Account;
	FILE* pFile = nullptr;
	pFile = fopen("Json.txt", "wb");
	if (pFile == nullptr)
		return 1;
	srand((unsigned int)time(NULL));
	Account.insert({ rand() % 1000, L"joshua" });
	Account.insert({ rand() % 1000, L"fernandes" });
	Account.insert({ rand() % 1000, L"dynames" });
	//rapidjson::GenericStringBuffer<rapidjson::UTF16<>> temp;

	rapidjson::StringBuffer StringJson;
	rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF16<>> writer(StringJson);
	//rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>, rapidjson::UTF16<>> writer(temp);

	writer.StartObject();
	writer.String(L"Account");
	writer.StartArray();

	for (std::map<int, std::wstring>::iterator itor = Account.begin(); itor != Account.end(); itor++)
	{
		writer.StartObject();
		writer.String(L"AccountNo");
		writer.Int64(itor->first);
		writer.String(L"NickName");
		writer.String((itor->second.c_str()));
		writer.EndObject();
	}
	writer.EndArray();
	writer.EndObject();

	int size = StringJson.GetSize();
	const char* pJson = StringJson.GetString();

	fwrite(pJson, size, 1, pFile);
	fclose(pFile);



	pFile = fopen("Json.txt", "rb");
	if (pFile == nullptr)
		return 1;


	fseek(pFile, 0, SEEK_END);
	int lSize = ftell(pFile);
	rewind(pFile);

	char* buffer = (char*)malloc(sizeof(char) * lSize + 1);

	int result = fread(buffer, 1, size, pFile);
	if (result != lSize)
		return 1;

	buffer[lSize] = 0;

	rapidjson::Document Doc;
	Doc.Parse(buffer);
	if (Doc.HasParseError())
		return 1;

	UINT64 AccountNo;

	std::map<int, std::wstring> temp12;


	rapidjson::Value& AccountArray = Doc["Account"];
	WCHAR szNickName[100];

	for (rapidjson::SizeType i = 0; i < AccountArray.Size(); i++)
	{
		rapidjson::Value& AccountObejct = AccountArray[i];
		AccountNo = AccountObejct["AccountNo"].GetUint64();
		int len = MultiByteToWideChar(CP_UTF8, 0, AccountObejct["NickName"].GetString(), strlen(AccountObejct["NickName"].GetString()), szNickName, 100);
		szNickName[len] = L'\0';
		temp12.insert({ AccountNo, szNickName });
	}


	for (std::map<int, std::wstring>::iterator itor = temp12.begin(); itor != temp12.end(); itor++)
	{
		wprintf(L"%d\n", itor->first);
		wprintf(L"%s\n", itor->second.c_str());
	}

	fclose(pFile);
	free(buffer);
	return 0;
}