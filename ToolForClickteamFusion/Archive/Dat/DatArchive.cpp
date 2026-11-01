#include "DatArchive.h"
#include "../../Util/DecryptionUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Archive;
using namespace CTT::DecryptionUtil;
using namespace CTT::CommonUtil;


DatArchive::DatArchive(wstring archivePath)
	: Archive(archivePath)
{
	archiveType = ArchiveType::ARCHIVE_DAT;
	engineType = EngineType::ENGINE_NORMAL;

	frontHeaderStartOffset = 0;
}


DatArchive::~DatArchive()
{
}


bool DatArchive::init()
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
	UINT32 archiveBackHeaderOffset = 0;

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

	// dll은 무시하고 데이터로 바로 간다
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