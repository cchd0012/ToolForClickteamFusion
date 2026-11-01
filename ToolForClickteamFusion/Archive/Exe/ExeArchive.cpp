#include "ExeArchive.h"
#include "../../Util/DecryptionUtil.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"
#include "../../Util/JsonUtil.h"

using namespace CTT::Archive;
using namespace CTT::DecryptionUtil;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;


ExeArchive::ExeArchive(wstring archivePath)
	: Archive(archivePath)
{
	archiveType = ArchiveType::ARCHIVE_EXE;
	engineType = EngineType::ENGINE_NORMAL;

	frontHeaderStartOffset = 0;
	modules.clear();
}


ExeArchive::~ExeArchive()
{
	for (size_t i = 0; i < modules.size();i++) {
		if (modules[i].data != NULL) {
			free(modules[i].data);
			modules[i].data = NULL;
		}
	}
	modules.clear();
}


bool ExeArchive::init() 
{
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	// exe 파일 열기
	hArchive = CreateFileW(archivePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hArchive == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 아카이브 파일을 열지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"아카이브 파일을 열지 못했습니다");
		return false;
	}

	DWORD dRead;

	// 필요 변수
	UINT32 archiveSize = 0;
	UINT32 archiveBackHeaderOffset = 0;

	// DOS 헤더 읽기
	IMAGE_DOS_HEADER dosHeader;
	ReadFile(hArchive, &dosHeader, sizeof(IMAGE_DOS_HEADER), &dRead, NULL);
	if (dRead != sizeof(IMAGE_DOS_HEADER)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": DOS 헤더를 읽지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"DOS 헤더를 읽지 못했습니다");
		return false;
	}

	// PE 헤더 위치로 이동
	SetFilePointer(hArchive, dosHeader.e_lfanew, NULL, FILE_BEGIN);

	// PE 파일 헤더 읽고 OptionalHeader는 그냥 옮기기. 다 읽으니까 꼬임
	IMAGE_NT_HEADERS ntHeaders;
	ReadFile(hArchive, &ntHeaders.Signature, sizeof(DWORD), &dRead, NULL);
	if (dRead != sizeof(DWORD)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": PE 헤더 시그니처를 읽지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"PE 헤더 시그니처를 읽지 못했습니다");
		return false;
	}
	ReadFile(hArchive, &ntHeaders.FileHeader, sizeof(IMAGE_FILE_HEADER), &dRead, NULL);
	if (dRead != sizeof(IMAGE_FILE_HEADER)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": PE 파일 헤더를 읽지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"PE 파일 헤더를 읽지 못했습니다");
		return false;
	}
	SetFilePointer(hArchive, ntHeaders.FileHeader.SizeOfOptionalHeader, NULL, FILE_CURRENT);

	// 제일 마지막 섹션 그 다음 위치를 찾는다
	IMAGE_SECTION_HEADER sectionHeader;
	for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; ++i) {
		ReadFile(hArchive, &sectionHeader, sizeof(IMAGE_SECTION_HEADER), &dRead, NULL);
		if (dRead != sizeof(IMAGE_SECTION_HEADER)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 섹션 헤더를 읽지 못했습니다." << endl;
			triggerError(__FUNCTION__, u8"섹션 헤더를 읽지 못했습니다");
			return false;
		}
		if (frontHeaderStartOffset < (sectionHeader.PointerToRawData + sectionHeader.SizeOfRawData)) {
			frontHeaderStartOffset = sectionHeader.PointerToRawData + sectionHeader.SizeOfRawData;
		}
	}

	// 프론트 헤더 읽어들이기
	SetFilePointer(hArchive, frontHeaderStartOffset, NULL, FILE_BEGIN);
	ReadFile(hArchive, &frontHeader, sizeof(FrontHeader), &dRead, NULL);
	if (dRead != sizeof(FrontHeader)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 패킹된 데이터가 없습니다." << endl;
		triggerError(__FUNCTION__, u8"패킹된 데이터가 없습니다");
		return false;
	}

	// 헤더 검증, firstMagic 값이 [77 77 77 77]인 것만 읽는다는 것은 곧 normal한 것만 읽겠다는 뜻이다
	if (frontHeader.firstMagic != FIRST_MAGIC_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 데이터가 깨졌습니다 [MGC-1]." << endl;
		triggerError(__FUNCTION__, u8"데이터가 깨졌습니다 [MGC-1]");
		return false;
	}
	if (frontHeader.secondMagic != SECOND_MAGIC_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 데이터가 깨졌습니다 [MGC-2]." << endl;
		triggerError(__FUNCTION__, u8"데이터가 깨졌습니다 [MGC-2]");
		return false;
	}
	if (frontHeader.headerSize != sizeof(FrontHeader)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 헤더 크기가 맞지 않습니다." << endl;
		triggerError(__FUNCTION__, u8"헤더 크기가 맞지 않습니다");
		return false;
	}

	// module 데이터 읽기
	if (!readModulesFromArchive()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": module 데이터를 읽어들이지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"module 데이터를 읽어들이지 못했습니다");
		return false;
	}

	// 백 헤더 위치
	archiveBackHeaderOffset = frontHeaderStartOffset + (frontHeader.moduleDataSize - frontHeader.headerSize);

	// 백 헤더 읽기
	SetFilePointer(hArchive, archiveBackHeaderOffset, NULL, FILE_BEGIN);
	ReadFile(hArchive, &backHeader, sizeof(BackHeader), &dRead, NULL);
	if (dRead != sizeof(BackHeader)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 백 헤더를 읽지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"백 헤더를 읽지 못했습니다");
		return false;
	}

	// 백 헤더 검증, firstMagic 값이 "PAMU"인 것만 읽는다는 것은 곧 유니코드한 normal 데이터만 읽겠다는 뜻이다
	if (backHeader.firstMagic != PAMU_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 해당 아카이브는 지원하지 않습니다." << endl;
		triggerError(__FUNCTION__, u8"해당 아카이브는 지원하지 않습니다");
		return false;
	}

	// chunk 시작위치 저장 및 App 정보 세팅
	chunkStartOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);
	if (!setApplicationInfo()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Application 정보를 얻지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"Application 정보를 얻지 못했습니다");
		return false;
	}

	// 복호화/암호화용 테이블 초기화 후 리턴
	initializeDecryptionTable();
	initializeEncryptionTable();

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;
	return true;
}


bool ExeArchive::makeModuleReplacedArchive()
{
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	// 새 파일 핸들 열기
	wstring newArchivePath = getExtentionRemovedPath(archivePath) + L"_new." + getExtention(archivePath);
	_wremove(newArchivePath.c_str());
	HANDLE hNewArchive = CreateFileW(newArchivePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hNewArchive == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(newArchivePath) << L": 아카이브 파일을 열지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"아카이브 파일을 열지 못했습니다");
		return false;
	}

	// 모듈과 백헤더 기록
	writeModulesIntoArchive(hNewArchive);

	// 기존 hArchive의 백 헤더 위치
	UINT32 archiveBackHeaderOffset = frontHeaderStartOffset + (frontHeader.moduleDataSize - frontHeader.headerSize);

	// 백헤더는 이미 기록했으니 위치를 이동한다
	UINT32 archiveAfterBackHeaderOffset = archiveBackHeaderOffset + sizeof(BackHeader);

	// hArchive 핸들 이동 후 나머지 데이터도 전부 읽어들여서 기록, 용량이 클 가능성이 높으니 스트림 형식으로 옮긴다
	SetFilePointer(hArchive, archiveAfterBackHeaderOffset, NULL, FILE_BEGIN);
	UINT32 tmpAfterModuleSize = GetFileSize(hArchive, NULL) - archiveAfterBackHeaderOffset;
	if (!segmentedTransferData(hArchive, hNewArchive, tmpAfterModuleSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(newArchivePath) << L": module 이외의 데이터를 옮기지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"module 이외의 데이터를 옮기지 못했습니다");
		return false;
	}

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;

	return true;
}


bool ExeArchive::writeModulesIntoArchive(HANDLE hNewArchive)
{
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	DWORD dRead, dWrite;

	// frontHeaderStartOffset 이전까지 읽어들였다가 새 핸들에 기록
	SetFilePointer(hArchive, 0, NULL, FILE_BEGIN);
	UINT32 tmpBeforeModuleSize = frontHeaderStartOffset;
	BYTE* tmpBeforeModuleBuffer = (BYTE*)malloc(tmpBeforeModuleSize);
	ReadFile(hArchive, tmpBeforeModuleBuffer, tmpBeforeModuleSize, &dRead, NULL);
	WriteFile(hNewArchive, tmpBeforeModuleBuffer, tmpBeforeModuleSize, &dWrite, NULL);
	free(tmpBeforeModuleBuffer);

	// 크기 계산 및 버퍼 새로 만들기 (압축하지 않는다는 가정하의 버퍼)
	UINT32 firstModuleBufferSize = 0;
	for (size_t i = 0; i < modules.size(); i++) {
		firstModuleBufferSize += sizeof(UINT16);
		firstModuleBufferSize += (sizeof(WCHAR) * (UINT32)modules[i].name.size());
		firstModuleBufferSize += sizeof(UINT32);
		firstModuleBufferSize += sizeof(UINT32);
		firstModuleBufferSize += modules[i].size;
	}

	// 버퍼 할당
	BYTE* moduleRawBuffer = (BYTE*)malloc(firstModuleBufferSize);
	memset(moduleRawBuffer, 0, firstModuleBufferSize);

	// 실제로 압축해가며 추가
	BYTE* moduleRawBufferPnt = moduleRawBuffer;
	for (size_t i = 0; i < modules.size(); i++) {

		*(UINT16*)(moduleRawBufferPnt) = (UINT16)(modules[i].name.size());
		moduleRawBufferPnt += sizeof(UINT16);

		memcpy(moduleRawBufferPnt, modules[i].name.c_str(), (sizeof(WCHAR) * modules[i].name.size()));
		moduleRawBufferPnt += (sizeof(WCHAR) * modules[i].name.size());

		*(UINT32*)(moduleRawBufferPnt) = modules[i].checksum;
		moduleRawBufferPnt += sizeof(UINT32);

		// "mmfs2.dll"이 아니면 전부 압축
		if (modules[i].name == L"mmfs2.dll") {

			*(UINT32*)(moduleRawBufferPnt) = modules[i].size;
			moduleRawBufferPnt += sizeof(UINT32);

			memcpy(moduleRawBufferPnt, modules[i].data, modules[i].size);
			moduleRawBufferPnt += modules[i].size;

		}
		else {

			UINT32 compressedSize = 0;
			BYTE* compressedData = NULL;
			if (!ZlibUtil::encode(modules[i].data, modules[i].size, compressedData, compressedSize)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << modules[i].name << L": 인코딩된 데이터를 반영하지 못했습니다." << endl;
				triggerError(__FUNCTION__, u8"인코딩된 데이터를 반영하지 못했습니다");
				if (compressedData != NULL) {
					free(compressedData);
				}
				free(moduleRawBuffer);
				return false;
			}

			*(UINT32*)(moduleRawBufferPnt) = compressedSize;
			moduleRawBufferPnt += sizeof(UINT32);

			memcpy(moduleRawBufferPnt, compressedData, compressedSize);
			moduleRawBufferPnt += compressedSize;

			free(compressedData);

		}

	}

	// 실제 버퍼 크기
	UINT32 realModuleBufferSize = (UINT32)(moduleRawBufferPnt - moduleRawBuffer);

	// 기존 hArchive의 백 헤더 위치
	UINT32 archiveBackHeaderOffset = frontHeaderStartOffset + (frontHeader.moduleDataSize - frontHeader.headerSize);

	// 수정된 frontHeader 및 module 버퍼 기록
	frontHeader.moduleDataSize = realModuleBufferSize + frontHeader.headerSize + sizeof(FrontHeader);
	frontHeader.moduleCount = (UINT32)modules.size();
	WriteFile(hNewArchive, &frontHeader, sizeof(FrontHeader), &dWrite, NULL);
	WriteFile(hNewArchive, moduleRawBuffer, realModuleBufferSize, &dWrite, NULL);
	free(moduleRawBuffer);

	// 백헤드 버퍼 기록
	WriteFile(hNewArchive, &backHeader, sizeof(BackHeader), &dWrite, NULL);

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;

	return true;
}


bool ExeArchive::exportModules()
{
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	DWORD dWrite;

	wstring moduleDirPath = getExtentionRemovedPath(archivePath) + L"/modules";
	createDirectoryRecursively(moduleDirPath);

	for (size_t i = 0; i < modules.size();i++) {

		// 추출 개수 갱신
		getStates()->currentDataIndex++;

		// 경로 생성
		wostringstream woss;
		woss << setw(4) << setfill(L'0') << i;
		wostringstream woss2;
		woss2 << uppercase << setw(8) << setfill(L'0') << hex << modules[i].checksum;
		wstring modulePath = woss.str() + L"_" + woss2.str() + L"_" + modules[i].name;
		wstring moduleFullPath = moduleDirPath + L"/" + modulePath;

		wcout << infoPrefix << modulePath << L" writing..." << endl;

		HANDLE hModuleWrite = CreateFileW(moduleFullPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		WriteFile(hModuleWrite, modules[i].data, modules[i].size, &dWrite, NULL);
		CloseHandle(hModuleWrite);

	}

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;

	return true;
}


bool ExeArchive::importModules()
{
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	DWORD dRead;

	wstring moduleDirPath = getExtentionRemovedPath(archivePath) + L"/modules";
	if (isFileOrDir(moduleDirPath) != FSOType::FSO_DIRECTORY) {
		wcout << infoPrefix << moduleDirPath << L": 디렉토리가 존재하지 않습니다." << endl;
		wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;
		return true;
	}

	for (size_t i = 0; i < modules.size(); i++) {
		if (modules[i].data != NULL) {
			free(modules[i].data);
			modules[i].data = NULL;
		}
	}
	modules.clear();

	HANDLE hModuleSearch;
	WIN32_FIND_DATAW wfd;
	wstring searchPattern = moduleDirPath + L"/*";

	// modules 디렉토리 안에 있는 모든 파일 검색 (디렉토리 제외)
	hModuleSearch = FindFirstFileW(searchPattern.c_str(), &wfd);
	if (hModuleSearch != INVALID_HANDLE_VALUE) {
		do {
			if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {

				wcout << infoPrefix << wfd.cFileName << L" reading..." << endl;
				wstring moduleFilePath = moduleDirPath + L"/" + wfd.cFileName;
				HANDLE hModuleRead = CreateFileW(moduleFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

				wstring indexStr, checksumHexStr, onlyName;
				splitTwice(wfd.cFileName, indexStr, checksumHexStr, onlyName);

				if (indexStr.empty() || checksumHexStr.empty() || onlyName.empty()) {
					wcout << infoPrefix << wfd.cFileName << L": 해당 파일은 양식에 맞지 않아 반영하지 않습니다." << endl;
					CloseHandle(hModuleRead);
				}
				else {

					UINT32 index = (UINT32)stoul(indexStr, NULL);
					UINT32 checksum = (UINT32)stoul(checksumHexStr, NULL, 16);
					UINT32 moduleSize = GetFileSize(hModuleRead, NULL);
					BYTE* moduleBuffer = (BYTE*)malloc(moduleSize);
					ReadFile(hModuleRead, moduleBuffer, moduleSize, &dRead, NULL);
					CloseHandle(hModuleRead);

					CTT::Archive::Module tmpModule;

					tmpModule.name = onlyName;
					tmpModule.checksum = checksum;
					tmpModule.data = moduleBuffer;
					tmpModule.size = moduleSize;

					modules.push_back(tmpModule);

				}

			}
		} while (FindNextFileW(hModuleSearch, &wfd) != 0);
	}

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;

	return true;
}


bool ExeArchive::readModulesFromArchive()
{
	DWORD dRead;

	for (size_t i = 0; i < modules.size(); i++) {
		if (modules[i].data != NULL) {
			free(modules[i].data);
			modules[i].data = NULL;
		}
	}
	modules.clear();

	SetFilePointer(hArchive, frontHeaderStartOffset + frontHeader.headerSize, NULL, FILE_BEGIN);

	UINT32 archiveBackHeaderOffset = frontHeaderStartOffset + (frontHeader.moduleDataSize - frontHeader.headerSize);
	UINT32 moduleRawBufferSize = archiveBackHeaderOffset - (frontHeaderStartOffset + frontHeader.headerSize);
	BYTE* moduleRawBuffer = (BYTE*)malloc(moduleRawBufferSize);
	ReadFile(hArchive, moduleRawBuffer, moduleRawBufferSize, &dRead, NULL);
	if (dRead != moduleRawBufferSize) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": module 정보를 읽어들이지 못했습니다." << endl;
		triggerError(__FUNCTION__, u8"module 정보를 읽어들이지 못했습니다");
		free(moduleRawBuffer);
		return false;
	}

	BYTE* moduleRawBufferPnt = moduleRawBuffer;
	for (UINT32 i = 0; i < frontHeader.moduleCount; i++) {
		
		Module tmpModule;

		UINT16 nameLength = *(UINT16*)moduleRawBufferPnt;
		moduleRawBufferPnt += sizeof(UINT16);

		tmpModule.name.resize(nameLength);
		memcpy((void*)tmpModule.name.c_str(), moduleRawBufferPnt, (sizeof(WCHAR) * nameLength));
		moduleRawBufferPnt += (sizeof(WCHAR) * nameLength);

		tmpModule.checksum = *(UINT32*)moduleRawBufferPnt;
		moduleRawBufferPnt += sizeof(UINT32);

		// "mmfs2.dll"이 아니면 전부 압축되어 있음
		if (tmpModule.name == L"mmfs2.dll") {
			tmpModule.size = *(UINT32*)moduleRawBufferPnt;
			moduleRawBufferPnt += sizeof(UINT32);

			tmpModule.data = (BYTE*)malloc(tmpModule.size);
			memcpy(tmpModule.data, moduleRawBufferPnt, tmpModule.size);
			moduleRawBufferPnt += tmpModule.size;
		}
		else {
			UINT32 compressedSize = *(UINT32*)moduleRawBufferPnt;
			moduleRawBufferPnt += sizeof(UINT32);

			BYTE *compressedData = (BYTE*)malloc(compressedSize);
			memcpy(compressedData, moduleRawBufferPnt, compressedSize);
			moduleRawBufferPnt += compressedSize;

			// 미리 크기를 정해둬야 할듯
			tmpModule.size = ((compressedSize * 3) + 0x500000);
			if (!ZlibUtil::decode(compressedData, compressedSize, tmpModule.data, tmpModule.size, false)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << tmpModule.name << L": module 정보를 디코딩하지 못했습니다." << endl;
				triggerError(__FUNCTION__, u8"module 정보를 디코딩하지 못했습니다");
				free(compressedData);
				return false;
			}
		}

		modules.push_back(tmpModule);

	}

	free(moduleRawBuffer);
	return true;
}