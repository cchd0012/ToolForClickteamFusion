#include "CcnArchive.h"
#include "../../Util/DecryptionUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Archive;
using namespace CTT::DecryptionUtil;
using namespace CTT::CommonUtil;


CcnArchive::CcnArchive(wstring archivePath)
	: Archive(archivePath)
{
	archiveType = ArchiveType::ARCHIVE_CCN;
	engineType = EngineType::ENGINE_NORMAL;
}


CcnArchive::~CcnArchive()
{
}


bool CcnArchive::init()
{
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	// ccn 파일 열기
	hArchive = CreateFileW(archivePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hArchive == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": 아카이브 파일을 열지 못했습니다.";
		triggerError(__FUNCTION__, u8"아카이브 파일을 열지 못했습니다");
		return false;
	}

	DWORD dRead;

	// 백 헤더 읽기
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

	// 복호화용/암호화용 테이블 초기화 후 리턴
	initializeDecryptionTable();
	initializeEncryptionTable();

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;

	return true;

}