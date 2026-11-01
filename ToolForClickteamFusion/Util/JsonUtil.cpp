#include "JsonUtil.h"
#include "CommonUtil.h"

using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;


bool CTT::JsonUtil::readFromJson(Json::Value& value, wstring& path)
{
	DWORD dRead;
	
	wstring jsonPath = path;

	// 디렉토리가 존재하는지 파악하기
	wstring dirPath = getOnlyDirPath(path);
	if (path != dirPath) {
		if (isFileOrDir(dirPath) != FSOType::FSO_DIRECTORY) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << dirPath << L": 디렉토리가 존재하지 않습니다" << endl;
			return false;
		}
	}

	// json 확장자 없으면 붙여주기
	if (jsonPath.rfind(L".json") == wstring::npos) {
		jsonPath += L".json";
	}

	// 핸들 만들기
	HANDLE hJson = CreateFileW(jsonPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	// 파일 크기만큼 읽어들이기
	DWORD jsonSize = GetFileSize(hJson, NULL);
	std::string input(jsonSize, 0);
	if (!ReadFile(hJson, &input[0], jsonSize, &dRead, NULL)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << jsonPath << L": json 데이터를 읽어들이지 못했습니다" << endl;
		return false;
	}
	CloseHandle(hJson);

	// json 파싱
	Json::CharReaderBuilder reader;
	std::stringstream ss(input);
	std::string errs;
	if (!Json::parseFromStream(reader, ss, &value, &errs)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << jsonPath << L": json 데이터 파싱 실패: " << getUniStr(errs, CodePage::CP_UTF_8) << endl;
		return false;
	}

	return true;
}


bool CTT::JsonUtil::writeIntoJson(Json::Value& value, wstring &path)
{
	DWORD dWrite;

	wstring jsonPath = path;

	// 먼저 디렉토리를 포함한 경로라면 디렉토리를 만들어야 한다
	wstring dirPath = getOnlyDirPath(path);
	if (path != dirPath) {
		if (!createDirectoryRecursively(dirPath)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << dirPath << L": 디렉토리를 생성하지 못했습니다" << endl;
			return false;
		}
	}

	// json 확장자 없으면 붙여주기
	if (jsonPath.rfind(L".json") == wstring::npos) {
		jsonPath += L".json";
	}

	// 핸들 만들기
	HANDLE hJson = CreateFileW(jsonPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	// 핸들에 기록
	Json::StreamWriterBuilder builder;
	builder.settings_["emitUTF8"] = true;
	string output = Json::writeString(builder, value);
	WriteFile(hJson, output.c_str(), output.size(), &dWrite, NULL);
	CloseHandle(hJson);

	return true;
}