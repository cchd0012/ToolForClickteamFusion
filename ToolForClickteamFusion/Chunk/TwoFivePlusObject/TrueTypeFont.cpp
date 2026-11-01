#include "TrueTypeFont.h"
#include "../../Archive/Archive.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;


TrueTypeFont::TrueTypeFont() {
	memset(&value, 0, sizeof(CompressedDataInfo));
}


TrueTypeFont::~TrueTypeFont() {
	if (value.compressedData != NULL) {
		free(value.compressedData);
		value.compressedData = NULL;
	}
}


bool TrueTypeFont::exportData(wstring& dirPath, UINT32 index, bool isFullyExport)
{
	DWORD dWrite;

	// 추출 파일 경로 생성
	wostringstream woss;
	woss << setw(6) << setfill(L'0') << index;
	wstring exportFilePath = dirPath + L"/" + woss.str() + L".ttf";

	wcout << infoPrefix << L"exporting " << exportFilePath << L"..." << endl;

	// 압축풀기
	BYTE* orgBuffer = NULL;
	if (!ZlibUtil::decode(value.compressedData, value.compressedSize, orgBuffer, value.orgSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType 폰트 데이터 디코딩을 실패했습니다. (buffer)" << endl;
		if (orgBuffer != NULL) {
			free(orgBuffer);
		}
		return false;
	}

	// 추출 후 정리
	HANDLE hWrite = CreateFileW(exportFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	WriteFile(hWrite, orgBuffer, value.orgSize, &dWrite, NULL);
	CloseHandle(hWrite);
	free(orgBuffer);

	return true;
}


bool TrueTypeFont::importData(wstring& dirPath, UINT32 index)
{
	DWORD dRead;

	// import 파일 경로 생성
	wostringstream woss;
	woss << setw(6) << setfill(L'0') << index;
	wstring importFilePath = dirPath + L"/" + woss.str() + L".ttf";

	// 존재 확인, 없으면 그냥 돌아감
	if (isFileOrDir(importFilePath) != FSOType::FSO_FILE) {
		return true;
	}

	wcout << infoPrefix << L"importing " << importFilePath << L"..." << endl;

	HANDLE hRead = CreateFileW(importFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hRead == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(importFilePath) << L": TrueType 폰트 파일을 읽어들이지 못했습니다." << endl;
		return false;
	}

	if (value.compressedData != NULL) {
		free(value.compressedData);
		value.compressedData = NULL;
	}

	value.orgSize = GetFileSize(hRead, NULL);
	BYTE* orgBuffer = (BYTE*)malloc(value.orgSize);
	memset(orgBuffer, 0, value.orgSize);
	ReadFile(hRead, orgBuffer, value.orgSize, &dRead, NULL);
	CloseHandle(hRead);

	if (!ZlibUtil::encode(orgBuffer, value.orgSize, value.compressedData, value.compressedSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType 폰트 데이터 인코딩을 실패했습니다. (buffer)" << endl;
		free(orgBuffer);
		if (value.compressedData != NULL) {
			free(value.compressedData);
			value.compressedData = NULL;
		}
		return false;
	}

	free(orgBuffer);

	return true;
}


bool TrueTypeFont::loadFromBuffer(BYTE*& bufferPnt)
{
	value.orgSize = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	value.compressedSize = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	value.compressedData = (BYTE*)malloc(value.compressedSize);
	memcpy(value.compressedData, bufferPnt, value.compressedSize);
	bufferPnt += value.compressedSize;

	return true;
}


bool TrueTypeFont::saveIntoBuffer(BYTE*& bufferPnt)
{
	*(UINT32*)bufferPnt = value.orgSize;
	bufferPnt += sizeof(UINT32);

	*(UINT32*)bufferPnt = value.compressedSize;
	bufferPnt += sizeof(UINT32);

	memcpy(bufferPnt, value.compressedData, value.compressedSize);
	bufferPnt += value.compressedSize;

	return true;
}


// 데이터 렌더링용 함수
void TrueTypeFont::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    blob_size: " << value.compressedSize << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    blob_org_size: " << value.orgSize;
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("  }");
}