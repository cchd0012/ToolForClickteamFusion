#include "BinaryFile.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


BinaryFile::BinaryFile() {
	binaryData = NULL;
	binarySize = 0;
}


BinaryFile::~BinaryFile() {
	if (binaryData != NULL) {
		free(binaryData);
		binaryData = NULL;
	}
}


bool BinaryFile::exportData(wstring& dirPath, UINT32 index, bool isFullyExport)
{
	// 추출 파일 경로 생성
	wstring exportFilePath = dirPath + L"/" + getOnlyName(name);

	wcout << infoPrefix << L"exporting " << exportFilePath << L"..." << endl;

	DWORD dWrite;

	// 그대로 기록
	HANDLE hBinaryFile = CreateFileW(exportFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hBinaryFile == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(exportFilePath) << L": 바이너리 파일을 추출하지 못했습니다." << endl;
		return false;
	}
	WriteFile(hBinaryFile, binaryData, binarySize, &dWrite, NULL);
	CloseHandle(hBinaryFile);

	return true;
}


bool BinaryFile::importData(wstring& dirPath, UINT32 index)
{
	// import할 파일 경로 생성
	wstring importFilePath = dirPath + L"/" + getOnlyName(name);

	// 존재 확인, 없으면 그냥 돌아감
	if (isFileOrDir(importFilePath) != FSOType::FSO_FILE) {
		return true;
	}

	wcout << infoPrefix << L"importing " << importFilePath << L"..." << endl;

	DWORD dRead;

	if (binaryData != NULL) {
		free(binaryData);
		binaryData = NULL;
	}
	binarySize = 0;

	// 그대로 읽어들이기
	HANDLE hBinaryFile = CreateFileW(importFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hBinaryFile == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(importFilePath) << L": 바이너리 파일을 읽어들이지 못했습니다." << endl;
		return false;
	}
	binarySize = GetFileSize(hBinaryFile, NULL);
	binaryData = (BYTE*)malloc(binarySize);
	ReadFile(hBinaryFile, binaryData, binarySize, &dRead, NULL);
	CloseHandle(hBinaryFile);

	return true;
}


bool BinaryFile::loadFromBuffer(BYTE*& bufferPnt)
{
	UINT16 nameLen = 0;
	memcpy(&nameLen, bufferPnt, sizeof(UINT16));
	bufferPnt += sizeof(UINT16);

	name.resize(nameLen);
	memcpy((PVOID)name.c_str(), bufferPnt, (nameLen * sizeof(WCHAR)));
	bufferPnt += (nameLen * sizeof(WCHAR));

	binarySize = *(UINT32*)(bufferPnt);
	bufferPnt += sizeof(UINT32);

	if (binaryData != NULL) {
		free(binaryData);
		binaryData = NULL;
	}

	binaryData = (BYTE*)malloc(binarySize);
	memcpy(binaryData, bufferPnt, binarySize);
	bufferPnt += binarySize;

	return true;
}


bool BinaryFile::saveIntoBuffer(BYTE*& bufferPnt)
{
	*(UINT16*)bufferPnt = (UINT16)(name.length());
	bufferPnt += sizeof(UINT16);

	memcpy(bufferPnt, name.c_str(), (name.length() * sizeof(WCHAR)));
	bufferPnt += (name.length() * sizeof(WCHAR));

	*(UINT32*)(bufferPnt) = binarySize;
	bufferPnt += sizeof(UINT32);

	memcpy(bufferPnt, binaryData, binarySize);
	bufferPnt += binarySize;

	return true;
}


// 데이터 렌더링용 함수
void BinaryFile::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    name: \"" << getAnsiStr(name, CodePage::CP_UTF_8) << "\",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    blob_size: " << binarySize;
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("  }");
}