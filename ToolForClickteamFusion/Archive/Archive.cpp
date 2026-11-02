#include "Archive.h"
#include "Exe/ExeArchive.h"
#include "Dat/DatArchive.h"
#include "Ccn/CcnArchive.h"
#include "../Application/Application.h"
#include "../Chunk/Image/Headers.h"
#include "../Chunk/Sound/Headers.h"
#include "../Chunk/Font/Headers.h"
#include "../Chunk/Object/Headers.h"
#include "../Chunk/TwoFivePlusObject/Headers.h"
#include "../Util/CommonUtil.h"
#include "../Util/DecryptionUtil.h"
#include "../Util/JsonUtil.h"

using namespace CTT::App;
using namespace CTT::Archive;
using namespace CTT::Chunk;
using namespace CTT::CommonUtil;
using namespace CTT::DecryptionUtil;
using namespace CTT::JsonUtil;




// 열려있는 아카이브 클래스 포인터 반환 함수
CTT::Archive::Archive* CTT::Archive::getArchive()
{
	return getApp()->getArchive();
}

// app 상태 포인터 반환 함수
ApplicationState* CTT::Archive::getStates()
{
	return getApp()->getStates();
}


// 에러 트리거 함수
// closeAllModal이 true면 현재 열린 모든 modal을 닫도록 조치한다. default는 false
void CTT::Archive::triggerError(const char* errTitle, const char* errMsg, bool closeAllModal)
{
	getApp()->triggerError(errTitle, errMsg, closeAllModal);
}




Archive::Archive(wstring archivePath)
{
	this->archivePath = archivePath;
	memset(&frontHeader, 0, sizeof(FrontHeader));
	memset(&backHeader, 0, sizeof(BackHeader));
	chunkStartOffset = 0;
	newChunkStartOffset = 0;
	hArchive = INVALID_HANDLE_VALUE;
	frameScriptContextLoaded = false;
	rewriteFrameIndex = 0;
}


Archive::~Archive() 
{
	if (hArchive != INVALID_HANDLE_VALUE) {
		CloseHandle(hArchive);
		hArchive = INVALID_HANDLE_VALUE;
	}
}


// 아카이브 외부로부터 appName을 입력받아 세팅
void Archive::setAppNameFromEditor(wstring appNameValue) 
{
	wcout << endl << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": appName <- \"" << appNameValue << "\"" << endl;
	appName.setValue(appNameValue);
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;
}


void Archive::setAppLocaleIdFromEditor(UINT32 localeIdValue)
{
	wcout << endl << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": appLocaleId <- \"" << localeIdValue << "\"" << endl;
	appLocaleId.setValue(localeIdValue);
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;
}


void Archive::setAppCodepageFromEditor(UINT32 codepageValue)
{
	wcout << endl << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": appCodepage <- \"" << codepageValue << "\"" << endl;
	appCodepage.setValue(codepageValue);
	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;
}




void Archive::setDataCount(vector<wstring>& extractingDataTypes)
{
	wcout << endl << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	DWORD dRead;
	ChunkInfo chunkInfo;

	// TTF 오프셋을 공유하기 위한 데이터
	vector<UINT32> ttfOffsetList;

	// 초기화 및 chunk 시작 위치 지정
	memset(&chunkInfo, 0, sizeof(ChunkInfo));
	SetFilePointer(hArchive, chunkStartOffset, NULL, FILE_BEGIN);

	// 현재 추출 데이터 인덱스 초기화
	getStates()->totalDataCount = 0;
	getStates()->currentDataIndex = 0;

	// 만일 exe이고 모듈 교체에 체크가 되어있다면 모듈도 개수에 추가한다
	if ((archiveType == ArchiveType::ARCHIVE_EXE) && (contains(extractingDataTypes, L"module"))) {
		ExeArchive* exeArcPtr = reinterpret_cast<ExeArchive*>(this);
		getStates()->totalDataCount += (UINT32)exeArcPtr->getModules().size();
	}

	// 추출할 전체 데이터 수 카운트
	while (1) {

		// chunk를 읽어들일 때를 대비해 위치 백업 필요
		UINT32 backupOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);
		ReadFile(hArchive, &chunkInfo, sizeof(ChunkInfo), &dRead, NULL);
		SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);

		// 읽어온 길이가 0이라면 그냥 나간다
		if (dRead != sizeof(ChunkInfo)) {
			break;
		}

		switch (chunkInfo.type)
		{

			// blobs
			case ChunkType::CHUNK_BINARY_FILE_DATA:
			{
				if (contains(extractingDataTypes, L"blobs")) {
					binaryFileData.readOnlyDataCount(hArchive);
				}
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// AppShader
			case ChunkType::CHUNK_SHADER_LIST:
			{
				if (contains(extractingDataTypes, L"appShader")) {
					shaderList.readOnlyDataCount(hArchive);
				}
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// Image
			case ChunkType::CHUNK_IMAGE_DATA:
			{
				if (contains(extractingDataTypes, L"image")) {
					ImageDataChunk* chunk = new ImageDataChunk();
					chunk->readOnlyDataCount(hArchive);
					delete chunk;
				}
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// Sound
			case ChunkType::CHUNK_SOUND_DATA:
			{
				if (contains(extractingDataTypes, L"sound")) {
					SoundDataChunk* chunk = new SoundDataChunk();
					chunk->readOnlyDataCount(hArchive);
					delete chunk;
				}
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// Object
			case ChunkType::CHUNK_OBJECT_PROPERTIES:
			{
				if (contains(extractingDataTypes, L"text") || contains(extractingDataTypes, L"script")) {
					ObjectDataChunk* chunk = new ObjectDataChunk();
					chunk->readOnlyDataCount(hArchive);
					delete chunk;
				}
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// Object 2.5+
			case ChunkType::CHUNK_2_5_TTF_DATA_LIST:
			{
				if (contains(extractingDataTypes, L"font")) {
					TrueTypeFontDataChunk* chunk = new TrueTypeFontDataChunk();
					chunk->readOnlyDataCount(hArchive);
					delete chunk;
				}
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}
			case ChunkType::CHUNK_2_5_TTF_INFO_LIST:
			{
				if (contains(extractingDataTypes, L"font")) {
					TrueTypeFontInfoDataChunk* chunk = new TrueTypeFontInfoDataChunk();
					chunk->readOnlyDataCount(hArchive);
					delete chunk;
				}
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// LogFont
			case ChunkType::CHUNK_LOG_FONT_DATA:
			{
				if (contains(extractingDataTypes, L"font")) {
					LogFontDataChunk* chunk = new LogFontDataChunk();
					chunk->readOnlyDataCount(hArchive);
					delete chunk;
				}
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// Frame (이벤트 텍스트)
			case ChunkType::CHUNK_FRAME:
			{
				if (contains(extractingDataTypes, L"text") || contains(extractingDataTypes, L"flow")) {
					getStates()->totalDataCount++;
				}
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// Object 2.5+ (오브젝트 텍스트 / 스크립트)
			case ChunkType::CHUNK_2_5_PLUS_OBJECT_DATA:
			{
				if (contains(extractingDataTypes, L"text") || contains(extractingDataTypes, L"script") || contains(extractingDataTypes, L"flow")) {
					getStates()->totalDataCount++;
				}
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

			// default
			default:
			{
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

		}

	}

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;
}




bool Archive::setApplicationInfo()
{
	DWORD dRead;
	ChunkInfo chunkInfo;

	// 초기화 및 chunk 시작 위치 지정
	memset(&chunkInfo, 0, sizeof(ChunkInfo));
	SetFilePointer(hArchive, chunkStartOffset, NULL, FILE_BEGIN);

	// chunk 트리 컨테이너도 초기화
	getStates()->chunkTreeNodeContainer.clear();

	// engine 버전 세팅
	while (1) {

		// chunk를 읽어들일 때를 대비해 위치 백업 필요
		UINT32 backupOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);

		// 더 이상 읽지 못하면 나가기
		ReadFile(hArchive, &chunkInfo, sizeof(ChunkInfo), &dRead, NULL);
		if (dRead != sizeof(ChunkInfo)) {
			break;
		}

		// 정보 추가
		ChunkTreeNode* node = new ChunkTreeNode();
		node->setValue(chunkInfo);
		node->setOffset(backupOffset);
		getStates()->chunkTreeNodeContainer.appendChildToCurrentNode(node);

		// 2.5+ Object chunk 여부에 따라 engine 버전 변경, 노드 세팅을 위해 일단은 그냥 넘김
		if ((chunkInfo.type == ChunkType::CHUNK_2_5_PLUS_OBJECT_HEADERS)
			|| (chunkInfo.type == ChunkType::CHUNK_2_5_PLUS_OBJECT_NAMES)
			|| (chunkInfo.type == ChunkType::CHUNK_2_5_PLUS_OBJECT_SHADERS)
			|| (chunkInfo.type == ChunkType::CHUNK_2_5_PLUS_OBJECT_DATA)
			|| (chunkInfo.type == ChunkType::CHUNK_2_5_PLUS_OBJECT_OFFSETS)) {
			engineType = EngineType::ENGINE_MMF_2_5_PLUS;
			// break;
		}

		// Frame Chunk면 어쩔 수 없이 한번 읽어들여줘야 한다
		if (chunkInfo.type == ChunkType::CHUNK_FRAME)
		{
			FrameChunk chunk;
			SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
			if (!chunk.readChunkTreeInfoFromArchive(hArchive)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": frame chunk 정보를 읽어들이지 못했습니다." << endl;
				return false;
			}
			SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
			ReadFile(hArchive, &chunkInfo, sizeof(ChunkInfo), &dRead, NULL);
		}

		SetFilePointer(hArchive, chunkInfo.size, NULL, FILE_CURRENT);

	}

	// 아카이브/엔진 버전까지 세팅 후 다시 돌기
	// 여기까지 오면 chunk는 다 돌 수 있다
	SetFilePointer(hArchive, chunkStartOffset, NULL, FILE_BEGIN);

	while (1) {

		// chunk를 읽어들일 때를 대비해 위치 백업 필요
		UINT32 backupOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);

		// 더 이상 읽지 못하면 나가기
		ReadFile(hArchive, &chunkInfo, sizeof(ChunkInfo), &dRead, NULL);
		if (dRead != sizeof(ChunkInfo)) {
			break;
		}

		// app에 관련된 정보들은 읽어들이고, 그렇지 않다면 그냥 넘긴다
		switch (chunkInfo.type)
		{
			case ChunkType::CHUNK_APP_NAME:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!appName.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Application Name을 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_APP_EDITOR_FILENAME:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!editorFileName.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Editor FileName을 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_COPYRIGHT:
			{
				// copyright의 value가 빈값이면 애초에 archive 자체에 없는 것으로 간주한다
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!copyright.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Copyright를 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_APP_HEADER:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!appHeader.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Application Header를 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_PRODUCT_VERSION_INFO:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!productVersionInfo.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Product Version 정보를 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_PRODUCT_BUILD_VERSION_INFO:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!productBuildVersionInfo.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Product Build Version 정보를 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_APP_LOCALE_ID:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!appLocaleId.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Application Locale Id를 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_APP_CODEPAGE:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!appCodepage.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Application Codepage를 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_PROTECTION:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!protection.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Protection 값을 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_BINARY_FILE_DATA:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!binaryFileData.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Binary Data를 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_SHADER_LIST:
			{
				SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
				if (!shaderList.readFromArchive(hArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(archivePath) << L": Shader 리스트를 읽어들이지 못했습니다." << endl;
					return false;
				}
				break;
			}
			default:
			{
				SetFilePointer(hArchive, chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}
		}

	}

	return true;
}




bool Archive::exportFromArchive(vector<wstring>& extractingDataTypes)
{
	wcout << endl << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	DWORD dRead;
	ChunkInfo chunkInfo;

	// 만일 exe이고 모듈 교체에 체크가 되어있다면 ExeArchive로 바꾼다음 모듈 추출을 수행한다
	if ((archiveType == ArchiveType::ARCHIVE_EXE) && (contains(extractingDataTypes, L"module"))) {

		wcout << L"--------------------------------------" << endl;
		wcout << infoPrefix << L"modules extracting..." << endl;

		ExeArchive *exeArcPtr = reinterpret_cast<ExeArchive*>(this);
		if (!exeArcPtr->exportModules()) {
			return false;
		}

		wcout << infoPrefix << L"modules extracting complete!" << endl;
		wcout << L"--------------------------------------" << endl << endl;

	}

	// TTF 오프셋을 공유하기 위한 데이터
	vector<UINT32> ttfOffsetList;

	// 프레임 이벤트 텍스트를 모으기 위한 변수
	UINT32 frameIndex = 0;
	frameEventTextTable.clear();
	objectTextTable.clear();
	frameEventTextFlow.clear();
	frameScriptContext.objectNames.clear();
	frameScriptContextLoaded = false;

	// 초기화 및 chunk 시작 위치 지정
	memset(&chunkInfo, 0, sizeof(ChunkInfo));
	SetFilePointer(hArchive, chunkStartOffset, NULL, FILE_BEGIN);

	while (1) {

		// chunk를 읽어들일 때를 대비해 위치 백업 필요
		UINT32 backupOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);
		ReadFile(hArchive, &chunkInfo, sizeof(ChunkInfo), &dRead, NULL);
		SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);

		// 읽어온 길이가 0이라면 그냥 나간다
		if (dRead != sizeof(ChunkInfo)) {
			break;
		}

		switch (chunkInfo.type)
		{
			// blobs
			case ChunkType::CHUNK_BINARY_FILE_DATA:
			{
				if (contains(extractingDataTypes, L"blobs")) {
					wcout << L"--------------------------------------" << endl;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting..." << endl;
					if (!binaryFileData.readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 바이너리 데이터를 읽지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"바이너리 데이터를 읽지 못했습니다");
						return false;
					}
					wstring exportDirPath = getExtentionRemovedPath(archivePath) + L"/binaries";
					if (!binaryFileData.exportData(exportDirPath)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 바이너리 데이터를 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"바이너리 데이터를 추출하지 못했습니다");
						return false;
					}
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting complete!" << endl;
					wcout << L"--------------------------------------" << endl << endl;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// AppShader (추출만 가능)
			case ChunkType::CHUNK_SHADER_LIST:
			{
				if (contains(extractingDataTypes, L"appShader")) {
					wcout << L"--------------------------------------" << endl;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting..." << endl;
					if (!shaderList.readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Shader 데이터를 읽지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Shader 데이터를 읽지 못했습니다");
						return false;
					}
					wstring exportDirPath = getExtentionRemovedPath(archivePath) + L"/shaders";
					if (!shaderList.exportData(exportDirPath)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Shader 데이터를 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Shader 데이터를 추출하지 못했습니다");
						return false;
					}
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting complete!" << endl;
					wcout << L"--------------------------------------" << endl << endl;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// Image
			case ChunkType::CHUNK_IMAGE_DATA:
			{
				if (contains(extractingDataTypes, L"image")) {
					wcout << L"--------------------------------------" << endl;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting..." << endl;
					ImageDataChunk* chunk = new ImageDataChunk();
					if (!chunk->readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image 데이터를 읽지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Image 데이터를 읽지 못했습니다");
						delete chunk;
						return false;
					}
					wstring exportDirPath = getExtentionRemovedPath(archivePath) + L"/image";
					if (!chunk->exportData(exportDirPath)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image 데이터를 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Image 데이터를 추출하지 못했습니다");
						delete chunk;
						return false;
					}
					// 이미지 목록(핸들 포함)도 함께 내보낸다. 애니메이션 프레임은 위치가 아니라 핸들로 이미지를 가리킨다
					wstring exportListPath = exportDirPath + L"/imageList.json";
					if (!chunk->exportImageList(exportListPath)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image 목록을 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Image 목록을 추출하지 못했습니다");
						delete chunk;
						return false;
					}
					delete chunk;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting complete!" << endl;
					wcout << L"--------------------------------------" << endl << endl;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// Sound
			case ChunkType::CHUNK_SOUND_DATA:
			{
				if (contains(extractingDataTypes, L"sound")) {
					wcout << L"--------------------------------------" << endl;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting..." << endl;
					SoundDataChunk* chunk = new SoundDataChunk();
					if (!chunk->readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Sound 데이터를 읽지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Sound 데이터를 읽지 못했습니다");
						delete chunk;
						return false;
					}
					wstring exportDirPath = getExtentionRemovedPath(archivePath) + L"/sound";
					if (!chunk->exportData(exportDirPath)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Sound 데이터를 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Sound 데이터를 추출하지 못했습니다");
						delete chunk;
						return false;
					}
					delete chunk;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting complete!" << endl;
					wcout << L"--------------------------------------" << endl << endl;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// Object
			case ChunkType::CHUNK_OBJECT_PROPERTIES:
			{
				if (contains(extractingDataTypes, L"text") || contains(extractingDataTypes, L"script")) {
					wcout << L"--------------------------------------" << endl;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting..." << endl;
					ObjectDataChunk* chunk = new ObjectDataChunk();
					if (!chunk->readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Object 데이터를 읽지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Object 데이터를 읽지 못했습니다");
						delete chunk;
						return false;
					}
					wstring exportDirPath = getExtentionRemovedPath(archivePath) + L"/object";
					// "object-full" 이라는 문자열이 들어오면 전부 추출한다
					if (!chunk->exportData(exportDirPath, contains(extractingDataTypes, L"object-full"))) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Object 데이터를 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Object 데이터를 추출하지 못했습니다");
						delete chunk;
						return false;
					}
					delete chunk;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting complete!" << endl;
					wcout << L"--------------------------------------" << endl << endl;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// Object 2.5+
			case ChunkType::CHUNK_2_5_TTF_DATA_LIST:
			{
				if (contains(extractingDataTypes, L"font")) {
					wcout << L"--------------------------------------" << endl;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting..." << endl;
					TrueTypeFontDataChunk* chunk = new TrueTypeFontDataChunk();
					if (!chunk->readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType Font 데이터를 읽지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"TrueType Font 데이터를 읽지 못했습니다");
						delete chunk;
						return false;
					}
					wstring exportDirPath = getExtentionRemovedPath(archivePath) + L"/font/ttf";
					if (!chunk->exportData(exportDirPath)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType Font 데이터를 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"TrueType Font 데이터를 추출하지 못했습니다");
						delete chunk;
						return false;
					}
					// 오프셋 공유 이후 릴리즈
					ttfOffsetList = chunk->getOffsetList();
					delete chunk;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting complete!" << endl;
					wcout << L"--------------------------------------" << endl << endl;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}
			case ChunkType::CHUNK_2_5_TTF_INFO_LIST:
			{
				if (contains(extractingDataTypes, L"font")) {
					wcout << L"--------------------------------------" << endl;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting..." << endl;
					TrueTypeFontInfoDataChunk* chunk = new TrueTypeFontInfoDataChunk();
					// 미리 오프셋 세팅 후 진행
					chunk->setOffsetList(ttfOffsetList);
					if (!chunk->readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType Font 정보를 읽지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"TrueType Font 정보를 읽지 못했습니다");
						delete chunk;
						return false;
					}
					wstring exportJsonPath = getExtentionRemovedPath(archivePath) + L"/font/ttf/info.json";
					if (!chunk->exportJson(exportJsonPath)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType Font 정보를 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"TrueType Font 정보를 추출하지 못했습니다");
						delete chunk;
						return false;
					}
					delete chunk;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting complete!" << endl;
					wcout << L"--------------------------------------" << endl << endl;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// LogFont
			case ChunkType::CHUNK_LOG_FONT_DATA:
			{
				if (contains(extractingDataTypes, L"font")) {
					wcout << L"--------------------------------------" << endl;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting..." << endl;
					LogFontDataChunk* chunk = new LogFontDataChunk();
					if (!chunk->readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": LogFont 데이터를 읽지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"LogFont 데이터를 읽지 못했습니다");
						delete chunk;
						return false;
					}
					wstring exportJsonPath = getExtentionRemovedPath(archivePath) + L"/font/logFontInfo.json";
					if (!chunk->exportJson(exportJsonPath)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": LogFont 데이터를 추출하지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"LogFont 데이터를 추출하지 못했습니다");
						delete chunk;
						return false;
					}
					delete chunk;
					wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" extracting complete!" << endl;
					wcout << L"--------------------------------------" << endl << endl;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// Frame (이벤트 텍스트 / 스크립트)
			case ChunkType::CHUNK_FRAME:
			{
				if (contains(extractingDataTypes, L"text") || contains(extractingDataTypes, L"script") || contains(extractingDataTypes, L"flow")) {
					FrameChunk chunk;
					if (!chunk.readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Frame 데이터를 읽어들이지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"Frame 데이터를 읽어들이지 못했습니다");
						return false;
					}
					if (contains(extractingDataTypes, L"text")) {
						if (!chunk.extractEventText(frameIndex, frameEventTextTable)) {
							wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Frame 이벤트 텍스트를 추출하지 못했습니다." << endl;
							triggerError(__FUNCTION__, u8"Frame 이벤트 텍스트를 추출하지 못했습니다");
							return false;
						}
					}
					if (contains(extractingDataTypes, L"script") || contains(extractingDataTypes, L"flow")) {
						// 오브젝트 이름은 처음 필요할 때 한 번만 읽는다 (이벤트의 오브젝트 번호에 이름을 붙이기 위함)
						if (!frameScriptContextLoaded) {
							frameScriptContextLoaded = true;
							vector<ObjectHeaderEntry> headers;
							if (!readObjectHeaders(headers, frameScriptContext.objectNames)) {
								wcout << infoPrefix << L"오브젝트 이름을 읽지 못해 이벤트에 오브젝트 이름을 붙이지 않습니다." << endl;
								frameScriptContext.objectNames.clear();
							}
						}
						// 프레임 하나당 json 파일 하나로 풀어낸다 (디컴파일)
						Json::Value scriptValue;
						if (!chunk.decompileScript(frameIndex, frameScriptContext, scriptValue)) {
							wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Frame 스크립트를 디컴파일하지 못했습니다." << endl;
							triggerError(__FUNCTION__, u8"Frame 스크립트를 디컴파일하지 못했습니다");
							return false;
						}
						if (contains(extractingDataTypes, L"flow")) {
							frameEventTextFlow.addFrameScript(frameIndex, scriptValue);
						}
						if (contains(extractingDataTypes, L"script")) {
							wostringstream indexStream;
							indexStream << setw(6) << setfill(L'0') << frameIndex;
							wstring exportJsonPath = getExtentionRemovedPath(archivePath) + L"/script/frame/" + indexStream.str() + L".json";
							// 조건/동작 하나가 한 줄이 되도록 압축해서 쓴다 (들여쓰기만 줄여도 크기가 절반 이하)
							if (!writeIntoJson(scriptValue, exportJsonPath, 700)) {
								wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Frame 스크립트를 json 으로 저장하지 못했습니다." << endl;
								triggerError(__FUNCTION__, u8"Frame 스크립트를 json 으로 저장하지 못했습니다");
								return false;
							}
						}
					}
					frameIndex++;
					getStates()->currentDataIndex++;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// Object 2.5+ (오브젝트 텍스트 / 스크립트)
			case ChunkType::CHUNK_2_5_PLUS_OBJECT_DATA:
			{
				if (contains(extractingDataTypes, L"text") || contains(extractingDataTypes, L"script") || contains(extractingDataTypes, L"flow")) {
					ObjectElementDataChunk chunk;
					if (!chunk.readFromArchive(hArchive)) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 속성 데이터를 읽어들이지 못했습니다." << endl;
						triggerError(__FUNCTION__, u8"오브젝트 속성 데이터를 읽어들이지 못했습니다");
						return false;
					}
					vector<ObjectHeaderEntry> headers;
					vector<string> names;
					if (!readObjectHeaders(headers, names)) {
						triggerError(__FUNCTION__, u8"오브젝트 헤더를 읽어들이지 못했습니다");
						return false;
					}
					vector<ObjectContainerElement>& elements = chunk.getContainer().getElements();
					if (headers.size() != elements.size()) {
						wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 헤더 수와 속성 데이터 수가 다릅니다." << endl;
						triggerError(__FUNCTION__, u8"오브젝트 헤더 수와 속성 데이터 수가 다릅니다");
						return false;
					}
					for (size_t i = 0; i < elements.size(); i++) {
						// 타입 0 ~ 1 (배경)은 ObjectCommon 이 없다
						if (headers[i].type < 2) {
							continue;
						}
						ObjectCommon common;
						string errorMessage;
						if (!common.parse(elements[i].data, ObjectCommonLayout::TWO_FIVE_PLUS, getBuildVersion(), errorMessage)) {
							wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 " << dec << i << L" 번을 해석하지 못해 건너뜁니다." << endl;
							continue;
						}
						string objectName = (i < names.size()) ? names[i] : string();
						if (contains(extractingDataTypes, L"text")) {
							objectTextTable.addObject((INT32)i, objectName, common);
						}
						if (contains(extractingDataTypes, L"flow")) {
							Json::Value flowScriptValue;
							decompileObjectScript((INT32)i, objectName, headers[i].type, common, flowScriptValue);
							frameEventTextFlow.addObjectScript(flowScriptValue);
						}
						if (contains(extractingDataTypes, L"script")) {
							// 오브젝트 하나당 json 파일 하나로 풀어낸다 (디컴파일)
							Json::Value scriptValue;
							decompileObjectScript((INT32)i, objectName, headers[i].type, common, scriptValue);
							wostringstream indexStream;
							indexStream << setw(6) << setfill(L'0') << i;
							wstring exportJsonPath = getExtentionRemovedPath(archivePath) + L"/script/object/" + indexStream.str() + L".json";
							if (!writeIntoJson(scriptValue, exportJsonPath)) {
								wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 스크립트를 json 으로 저장하지 못했습니다." << endl;
								triggerError(__FUNCTION__, u8"오브젝트 스크립트를 json 으로 저장하지 못했습니다");
								return false;
							}
						}
					}
					getStates()->currentDataIndex++;
				}
				else {
					SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				}
				break;
			}

			// default
			default:
			{
				SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);
				break;
			}

		}

	}

	// 모아 둔 프레임 이벤트 텍스트를 JSON 으로 저장한다
	if (contains(extractingDataTypes, L"text") && (frameEventTextTable.getEntryCount() > 0)) {
		Json::Value textValue;
		frameEventTextTable.exportJson(textValue);
		wstring exportJsonPath = getExtentionRemovedPath(archivePath) + L"/text/frameEventText.json";
		if (!writeIntoJson(textValue, exportJsonPath)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Frame 이벤트 텍스트를 json 으로 저장하지 못했습니다." << endl;
			triggerError(__FUNCTION__, u8"Frame 이벤트 텍스트를 json 으로 저장하지 못했습니다");
			return false;
		}
		wcout << infoPrefix << L"Frame 이벤트 텍스트 " << dec << frameEventTextTable.getEntryCount() << L"개를 추출했습니다." << endl;
	}

	// 모아 둔 오브젝트 텍스트를 JSON 으로 저장한다
	if (contains(extractingDataTypes, L"text") && (objectTextTable.getEntryCount() > 0)) {
		Json::Value textValue;
		objectTextTable.exportJson(textValue);
		wstring exportJsonPath = getExtentionRemovedPath(archivePath) + L"/text/objectText.json";
		if (!writeIntoJson(textValue, exportJsonPath)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 텍스트를 json 으로 저장하지 못했습니다." << endl;
			triggerError(__FUNCTION__, u8"오브젝트 텍스트를 json 으로 저장하지 못했습니다");
			return false;
		}
		wcout << infoPrefix << L"오브젝트 텍스트 " << dec << objectTextTable.getEntryCount() << L"개 오브젝트를 추출했습니다." << endl;
	}

	// 텍스트 흐름 분석 결과를 JSON 으로 저장한다
	if (contains(extractingDataTypes, L"flow") && (frameEventTextFlow.getBlitterCount() > 0)) {
		Json::Value flowValue;
		frameEventTextFlow.exportJson(flowValue);
		wstring exportJsonPath = getExtentionRemovedPath(archivePath) + L"/text/textFlow.json";
		if (!writeIntoJson(flowValue, exportJsonPath)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 텍스트 흐름을 json 으로 저장하지 못했습니다." << endl;
			triggerError(__FUNCTION__, u8"텍스트 흐름을 json 으로 저장하지 못했습니다");
			return false;
		}
		wcout << infoPrefix << L"텍스트 흐름: 블리터 " << dec << frameEventTextFlow.getBlitterCount() << L"개 (이름 " << flowValue["summary"]["blitters"].asUInt() << L"종), 텍스트 설정 동작 " << flowValue["summary"]["assignments"].asUInt() << L"개를 분석했습니다." << endl;
	}

	// 만일 개수를 다 세어도 합이 0이라면 그냥 1/1로 맞춘다
	if (getStates()->totalDataCount == 0) {
		getStates()->totalDataCount = 1;
		getStates()->currentDataIndex = 1;
	}

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;

	return true;
}




bool Archive::rewriteNewArchive(vector<wstring>& replacingDataTypes)
{
	wcout << endl << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": processing..." << endl;

	DWORD dRead, dWrite;

	// "frame" 개수 변경시에는 내부적으로 blobs를 변경하고 암호화 테이블을 초기화해야겠지만...
	// 어차피 frame 개수가 바뀔 것 같진 않으니까 신경은 안 써도 될것 같다

	// 새 파일 핸들 열기
	wstring newArchivePath = getExtentionRemovedPath(archivePath) + L"_new." + getExtention(archivePath);
	_wremove(newArchivePath.c_str());
	HANDLE hNewArchive = CreateFileW(newArchivePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hNewArchive == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(newArchivePath) << L": 아카이브 파일을 열지 못했습니다." << endl;
		string newArchivePathAnsi = getAnsiStr(newArchivePath, CodePage::CP_UTF_8);
		triggerError(u8"아카이브 저장", newArchivePathAnsi.c_str(), true);
		return false;
	}

	// 암호화 테이블 초기화
	initializeEncryptionTable();

	// 프레임 이벤트 텍스트와 오브젝트 텍스트 번역 표는 처음 필요할 때 다시 읽어 들인다
	frameEventTextTable.clear();
	objectTextTable.clear();
	frameEventTextFlow.clear();
	rewriteFrameIndex = 0;

	// chunkStartOffset : 현재 아카이브 데이터에서 chunk가 시작하는 위치

	if ((archiveType == ArchiveType::ARCHIVE_EXE) && (contains(replacingDataTypes, L"module"))) {
		// 만일 exe이고 모듈 교체에 체크가 되어있다면 ExeArchive 핸들로 바꾼다음 모듈 교체를 수행해야 한다
		ExeArchive* exeArcPtr = reinterpret_cast<ExeArchive*>(this);
		if (!exeArcPtr->importModules()) {
			return false;
		}
		// 모듈 데이터 및 백헤더를 기록해야 새로 아카이브를 만들 준비가 된다
		if (!exeArcPtr->writeModulesIntoArchive(hNewArchive)) {
			return false;
		}
		// 모듈을 다시 압축하면 모듈 영역의 크기가 달라지므로, 새 아카이브의 chunk 시작 위치는 방금 기록을 마친 위치이다
		newChunkStartOffset = SetFilePointer(hNewArchive, 0, NULL, FILE_CURRENT);
	}
	else {
		// chunkStartOffset 이전까지 읽어들였다가 새 핸들에 기록
		SetFilePointer(hArchive, 0, NULL, FILE_BEGIN);
		UINT32 tmpBeforeChunkSize = chunkStartOffset;
		BYTE* tmpBeforeChunkBuffer = (BYTE*)malloc(tmpBeforeChunkSize);
		ReadFile(hArchive, tmpBeforeChunkBuffer, tmpBeforeChunkSize, &dRead, NULL);
		WriteFile(hNewArchive, tmpBeforeChunkBuffer, tmpBeforeChunkSize, &dWrite, NULL);
		free(tmpBeforeChunkBuffer);
		newChunkStartOffset = chunkStartOffset;
	}

	// 모든 chunk를 다시 기록
	if (!rewriteAllChunk(hNewArchive, replacingDataTypes)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(newArchivePath) << L": Application 데이터 변경을 실패했습니다." << endl;
		CloseHandle(hNewArchive);
		return false;
	}

	// 정리
	CloseHandle(hNewArchive);

	wcout << infoPrefix << L"<<function>> " << __FUNCTIONW__ << L": complete!" << endl;

	return true;
}


bool Archive::rewriteAllChunk(HANDLE hNewArchive, vector<wstring>& replacingDataTypes)
{
	DWORD dRead;
	ChunkInfo chunkInfo;

	// TTF 오프셋을 공유하기 위한 데이터
	vector<UINT32> ttfOffsetList;

	// 마무리 chunk
	ChunkInfo endChunk;
	memset(&endChunk, 0, sizeof(ChunkInfo));
	endChunk.type = ChunkType::CHUNK_END;

	// 초기화 및 chunk 시작 위치 지정. 양쪽 HANDLE 똑같이
	memset(&chunkInfo, 0, sizeof(ChunkInfo));
	SetFilePointer(hArchive, chunkStartOffset, NULL, FILE_BEGIN);
	SetFilePointer(hNewArchive, newChunkStartOffset, NULL, FILE_BEGIN);

	// 일단 chunk 단위로 저장을 수행한다 치고 진행
	getStates()->totalSavingDataCount = (UINT32)getStates()->chunkTreeNodeContainer.getRootNodePointer()->getChildren().size();
	getStates()->currentSavingDataIndex = 0;

	while (1) {

		// chunk를 읽어들일 때를 대비해 위치 백업 필요
		UINT32 backupOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);
		ReadFile(hArchive, &chunkInfo, sizeof(ChunkInfo), &dRead, NULL);
		SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);

		// 읽어온 길이가 0이라면 그냥 나간다
		if (dRead != sizeof(ChunkInfo)) {
			break;
		}

		wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(chunkInfo.type) << L" reencrypting..." << endl;

		// appName, editorFileName, copyright는 기존 것은 읽기는 읽되, 변경한 데이터를 반영해줘야 한다
		// frame은 안쪽이 chunk 리스트로 분리되어 있어서 마찬가지로 재암호해야 함
		// protection에서는 체크섬 값 변경
		// 이외에는 기본적으로 UnknownChunk 처리하며, 교체할 데이터는 readFromFile 같은 걸로 처리하면 될 것 같음
		// 파일을 추가하는 것도 가능하게 만들어야 하는데...
		switch (chunkInfo.type)
		{
			// AppInfo
			case ChunkType::CHUNK_APP_NAME: 
			{
				AppNameChunk tmpChunk;
				tmpChunk.readFromArchive(hArchive);
				if (!appName.writeIntoArchive(hNewArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 App name을 기록하지 못해 변환을 실패했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_APP_EDITOR_FILENAME:
			{
				EditorFileNameChunk tmpChunk;
				tmpChunk.readFromArchive(hArchive);
				if (!editorFileName.writeIntoArchive(hNewArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 editor filename을 기록하지 못해 변환을 실패했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_COPYRIGHT:
			{
				CopyrightChunk tmpChunk;
				tmpChunk.readFromArchive(hArchive);
				if (!copyright.writeIntoArchive(hNewArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 copyright를 기록하지 못해 변환을 실패했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_APP_HEADER:
			{
				AppHeaderChunk tmpChunk;
				tmpChunk.readFromArchive(hArchive);
				if (!appHeader.writeIntoArchive(hNewArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 Application Header를 기록하지 못해 변환을 실패했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_PRODUCT_VERSION_INFO:
			{
				ProductVersionInfoChunk tmpChunk;
				tmpChunk.readFromArchive(hArchive);
				if (!productVersionInfo.writeIntoArchive(hNewArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 Product Version 정보를 기록하지 못해 변환을 실패했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_PRODUCT_BUILD_VERSION_INFO:
			{
				ProductBuildVersionInfoChunk tmpChunk;
				tmpChunk.readFromArchive(hArchive);
				if (!productBuildVersionInfo.writeIntoArchive(hNewArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 Product Build Version 정보를 기록하지 못해 변환을 실패했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_APP_LOCALE_ID:
			{
				AppLocaleIdChunk tmpChunk;
				tmpChunk.readFromArchive(hArchive);
				if (!appLocaleId.writeIntoArchive(hNewArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 Application Locale Id를 기록하지 못해 변환을 실패했습니다." << endl;
					return false;
				}
				break;
			}
			case ChunkType::CHUNK_APP_CODEPAGE:
			{
				AppCodepageChunk tmpChunk;
				tmpChunk.readFromArchive(hArchive);
				if (!appCodepage.writeIntoArchive(hNewArchive)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 Application Codepage를 기록하지 못해 변환을 실패했습니다." << endl;
					return false;
				}
				break;
			}

			// blobs
			case ChunkType::CHUNK_BINARY_FILE_DATA:
			{
				if (contains(replacingDataTypes, L"blobs")) {
					if (!rewriteBinaryFilesChunk(hNewArchive)) {
						return false;
					}
				}
				else {
					if (!rewriteUnknownChunk(hNewArchive)) {
						return false;
					}
				}
				break;
			}

			// *** ChunkType::CHUNK_SHADER_LIST는 하지 않는다 ***

			// Frame
			case ChunkType::CHUNK_FRAME:
			{
				if (contains(replacingDataTypes, L"text") || contains(replacingDataTypes, L"script")) {
					if (!rewriteFrameChunk(hNewArchive, replacingDataTypes)) {
						return false;
					}
				}
				else {
					if (!rewriteUnknownChunk(hNewArchive)) {
						return false;
					}
				}
				break;
			}

			// AppInfo - Protection, 어떤 경우에서든 수행해야 함
			case ChunkType::CHUNK_PROTECTION:
			{
				if (!rewriteProtectionChunk(hNewArchive)) {
					return false;
				}
				break;
			}

			// Image -> 기존 image Offset chunk 넘기고 새 image Offset chunk까지 만들어서 기록
			case ChunkType::CHUNK_IMAGE_DATA: {
				if (contains(replacingDataTypes, L"image")) {
					if (!rewriteImagesChunk(hNewArchive)) {
						return false;
					}
				}
				else {
					if (!rewriteUnknownChunk(hNewArchive)) {
						return false;
					}
				}
				break;
			}

			// Sound -> 기존 sound Offset chunk 넘기고 새 sound Offset chunk까지 만들어서 기록
			case ChunkType::CHUNK_SOUND_DATA: {
				if (contains(replacingDataTypes, L"sound")) {
					if (!rewriteSoundsChunk(hNewArchive)) {
						return false;
					}
				}
				else {
					if (!rewriteUnknownChunk(hNewArchive)) {
						return false;
					}
				}
				break;
			}

			// LogFont
			case ChunkType::CHUNK_LOG_FONT_DATA: {
				if (contains(replacingDataTypes, L"font")) {
					if (!rewriteLogFontsChunk(hNewArchive)) {
						return false;
					}
				}
				else {
					if (!rewriteUnknownChunk(hNewArchive)) {
						return false;
					}
				}
				break;
			}

			// Object 2.5+, TrueType Font Data / TrueType Font Info Data
			case ChunkType::CHUNK_2_5_TTF_DATA_LIST: {
				if (contains(replacingDataTypes, L"font")) {
					if (!rewriteTrueTypeFontsChunk(hNewArchive, ttfOffsetList)) {
						return false;
					}
				}
				else {
					if (!rewriteUnknownChunk(hNewArchive)) {
						return false;
					}
				}
				break;
			}
			case ChunkType::CHUNK_2_5_TTF_INFO_LIST: {
				if (contains(replacingDataTypes, L"font")) {
					if (!rewriteTrueTypeFontInfosChunk(hNewArchive, ttfOffsetList)) {
						return false;
					}
				}
				else {
					if (!rewriteUnknownChunk(hNewArchive)) {
						return false;
					}
				}
				break;
			}

			// Object 2.5+ 속성 데이터(0x2256) + 오프셋(0x2257), 오브젝트 텍스트 교체
			case ChunkType::CHUNK_2_5_PLUS_OBJECT_DATA: {
				if (contains(replacingDataTypes, L"text") || contains(replacingDataTypes, L"script")) {
					if (!rewriteObjectElementChunks(hNewArchive, replacingDataTypes)) {
						return false;
					}
				}
				else {
					if (!rewriteUnknownChunk(hNewArchive)) {
						return false;
					}
				}
				break;
			}

			// *** 확인용, 확인이 되면 지울것
			case ChunkType::CHUNK_2_5_UNKNOWN_1:
			case ChunkType::CHUNK_2_5_UNKNOWN_2:
			{
				if (!rewriteUnknownChunk(hNewArchive)) {
					return false;
				}
				break;
			}

			// End
			case ChunkType::CHUNK_END:
			{ 
				EndChunk chunk;
				chunk.readFromArchive(hArchive);
				chunk.writeIntoArchive(hNewArchive);
				break;
			}

			// 이외 모든 chunk, 혹은 교체 대상에 포함되지 않는 chunk
			default:
			{
				if (!rewriteUnknownChunk(hNewArchive)) {
					return false;
				}
				break;
			}
		}

		// 프로그레스 인덱스 갱신
		getStates()->currentSavingDataIndex++;

	}

	return true;
}


bool Archive::rewriteBinaryFilesChunk(HANDLE hNewArchive)
{
	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"Binary File Data Chunk rewriting processing..." << endl;

	// Binary 파일 chunk 읽어들이기
	if (!binaryFileData.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Binary File chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 교체 작업 수행
	wstring importDirPath = getExtentionRemovedPath(archivePath) + L"/binaries";
	if (!binaryFileData.importData(importDirPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Binary File 데이터 교체를 실패했습니다." << endl;
		return false;
	}

	// 기록하기
	if (!binaryFileData.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Binary File chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	wcout << infoPrefix << L"Binary File Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}


bool Archive::rewriteFrameChunk(HANDLE hNewArchive, vector<wstring>& replacingDataTypes)
{
	FrameChunk chunk;

	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"Frame Chunk rewriting processing..." << endl;

	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 기존 frame을 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	UINT32 currentFrameIndex = rewriteFrameIndex++;

	// 프레임 스크립트 교체 (리컴파일): script/frame/<프레임 번호 6자리>.json 이 있는 프레임만 반영한다
	if (contains(replacingDataTypes, L"script")) {
		wostringstream indexStream;
		indexStream << setw(6) << setfill(L'0') << currentFrameIndex;
		wstring scriptPath = getExtentionRemovedPath(archivePath) + L"/script/frame/" + indexStream.str() + L".json";
		if (isFileOrDir(scriptPath) == FSOType::FSO_FILE) {
			Json::Value scriptValue;
			if (!readFromJson(scriptValue, scriptPath)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << scriptPath << L": Frame 스크립트 json 을 읽어들이지 못했습니다." << endl;
				triggerError(__FUNCTION__, u8"Frame 스크립트 json 을 읽어들이지 못했습니다", true);
				return false;
			}
			string errorMessage;
			if (!chunk.recompileScript(scriptValue, errorMessage)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << scriptPath << L": " << wstring(errorMessage.begin(), errorMessage.end()) << endl;
				triggerError(__FUNCTION__, u8"Frame 스크립트를 리컴파일하지 못했습니다", true);
				return false;
			}
			wcout << infoPrefix << L"Frame " << dec << currentFrameIndex << L" 번 스크립트를 반영했습니다." << endl;
		}
	}

	// 프레임 이벤트 텍스트 교체
	if (contains(replacingDataTypes, L"text")) {
		if (!frameEventTextTable.isLoaded()) {
			wstring importJsonPath = getExtentionRemovedPath(archivePath) + L"/text/frameEventText.json";
			if (isFileOrDir(importJsonPath) != FSOType::FSO_FILE) {
				wcout << infoPrefix << L"Frame 이벤트 텍스트 json 이 없어 텍스트 교체를 건너뜁니다: " << importJsonPath << endl;
				frameEventTextTable.markLoaded();
			}
			else {
				Json::Value textValue;
				if (!readFromJson(textValue, importJsonPath) || !frameEventTextTable.importJson(textValue)) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Frame 이벤트 텍스트 json 을 읽어들이지 못했습니다." << endl;
					triggerError(__FUNCTION__, u8"Frame 이벤트 텍스트 json 을 읽어들이지 못했습니다", true);
					return false;
				}
				wcout << infoPrefix << L"Frame 이벤트 텍스트 " << dec << frameEventTextTable.getEntryCount() << L"개를 읽어들였습니다." << endl;
			}
		}
		UINT32 replacedCount = 0;
		if (!chunk.replaceEventText(frameEventTextTable, replacedCount)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Frame 이벤트 텍스트를 교체하지 못했습니다." << endl;
			triggerError(__FUNCTION__, u8"Frame 이벤트 텍스트를 교체하지 못했습니다", true);
			return false;
		}
		if (replacedCount > 0) {
			wcout << infoPrefix << L"Frame 이벤트 텍스트 " << dec << replacedCount << L"개를 교체했습니다." << endl;
		}
	}

	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 frame을 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	wcout << infoPrefix << L"Frame Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}


bool Archive::rewriteProtectionChunk(HANDLE hNewArchive)
{
	ProtectionChunk chunk;
	UINT32 newChecksum = 0;
	UINT32 backupNewOffset = SetFilePointer(hNewArchive, 0, NULL, FILE_CURRENT);

	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"Protection Chunk rewriting processing..." << endl;

	// Protection chunk 읽어들이기
	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 기존 protection을 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 여기서는 protection chunk를 기록하기 이전이므로 체크할 필요가 없다
	if (!caculateProtectionChecksum(hNewArchive, newChecksum, true)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 protection를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 원래 기록할 위치로 복구 후 값을 바꾼다
	SetFilePointer(hNewArchive, backupNewOffset, NULL, FILE_BEGIN);
	chunk.setValue(newChecksum);

	// 기록하기
	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 protection를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	wcout << infoPrefix << L"Protection Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}


bool Archive::rewriteImagesChunk(HANDLE hNewArchive)
{
	ImageDataChunk chunk;
	ImageOffsetsChunk offsetChunk;

	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"Image/ImageOffset Chunk rewriting processing..." << endl;

	// image chunk 읽어들이기
	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image Data chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// image offset chunk 읽어들이기
	if (!offsetChunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image Offset chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 교체 작업 수행 (이미지가 존재할 때만)
	wstring importDirPath = getExtentionRemovedPath(archivePath) + L"/image";
	if (!chunk.importData(importDirPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image 데이터 교체를 실패했습니다." << endl;
		return false;
	}

	// 새 이미지 추가 (appendImages.json 이 있을 때만)
	wstring appendManifestPath = importDirPath + L"/appendImages.json";
	if (isFileOrDir(appendManifestPath) == FSOType::FSO_FILE) {
		if (!chunk.appendImages(appendManifestPath, importDirPath)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image 추가를 실패했습니다." << endl;
			triggerError(__FUNCTION__, u8"Image 추가를 실패했습니다", true);
			return false;
		}
	}

	// chunk 기록과 동시에 offset array 생성
	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image Data chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// offset chunk에 변경점 반영
	chunk.fillOffsets(offsetChunk);

	// offset chunk 기록하기
	if (!offsetChunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Image Offset chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	wcout << infoPrefix << L"Image/ImageOffset Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}


bool Archive::rewriteSoundsChunk(HANDLE hNewArchive)
{
	SoundDataChunk chunk;
	SoundOffsetsChunk offsetChunk;

	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"Sound/SoundOffset Chunk rewriting processing..." << endl;

	// sound chunk 읽어들이기
	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Sound Data chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// sound offset chunk 읽어들이기
	if (!offsetChunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Sound Offset chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 교체 작업 수행 (사운드 파일이 존재할 때만)
	wstring importDirPath = getExtentionRemovedPath(archivePath) + L"/sound";
	if (!chunk.importData(importDirPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Sound 데이터 교체를 실패했습니다." << endl;
		return false;
	}

	// chunk 기록과 동시에 offset array 생성
	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Sound Data chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// offset chunk에 변경점 반영
	chunk.fillOffsets(offsetChunk);

	// offset chunk 기록하기
	if (!offsetChunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Sound Offset chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	wcout << infoPrefix << L"Sound/SoundOffset Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}



bool Archive::rewriteTrueTypeFontsChunk(HANDLE hNewArchive, vector<UINT32>& ttfOffsetList)
{
	TrueTypeFontDataChunk chunk;

	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"TrueType Font Data Chunk rewriting processing..." << endl;

	// TrueType 폰트 chunk 읽어들이기
	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType 폰트 chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 교체 작업 수행
	wstring importDirPath = getExtentionRemovedPath(archivePath) + L"/font/ttf";
	if (!chunk.importData(importDirPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType 폰트 데이터 교체를 실패했습니다." << endl;
		return false;
	}

	// 기록하기
	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType 폰트 chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 오프셋 추출해두기
	ttfOffsetList.resize(chunk.getOffsetList().size());
	memcpy(ttfOffsetList.data(), chunk.getOffsetList().data(), (sizeof(UINT32) * chunk.getOffsetList().size()));

	wcout << infoPrefix << L"TrueType Font Data Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}


bool Archive::rewriteTrueTypeFontInfosChunk(HANDLE hNewArchive, vector<UINT32>& ttfOffsetList)
{
	TrueTypeFontInfoDataChunk chunk;

	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"TrueType Font Info Chunk rewriting processing..." << endl;

	// TrueType 폰트 정보 chunk 읽어들이기
	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType 폰트 정보 chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// offset 리스트 선반영 후 교체 작업 수행
	wstring importJsonPath = getExtentionRemovedPath(archivePath) + L"/font/ttf/info.json";
	chunk.setOffsetList(ttfOffsetList);
	if (!chunk.importJson(importJsonPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType 폰트 정보 데이터 교체를 실패했습니다." << endl;
		return false;
	}

	// 기록하기
	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": TrueType 폰트 정보 chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	wcout << infoPrefix << L"TrueType Font Info Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}


bool Archive::rewriteLogFontsChunk(HANDLE hNewArchive)
{
	LogFontDataChunk chunk;
	LogFontOffsetsChunk offsetChunk;

	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"LogFont/LogFontOffset Chunk rewriting processing..." << endl;

	// LogFont chunk 읽어들이기
	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": LogFont Data chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// LogFont offset chunk 읽어들이기
	if (!offsetChunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": LogFont Offset chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 교체 작업 수행 (json 파일이 존재할 때만)
	wstring importJsonPath = getExtentionRemovedPath(archivePath) + L"/font/logFontInfo.json";
	if (!chunk.importJson(importJsonPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": LogFont 데이터 교체를 실패했습니다." << endl;
		return false;
	}

	// chunk 기록과 동시에 offset array 생성
	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": LogFont Data chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// offset chunk에 변경점 반영
	chunk.fillOffsets(offsetChunk);

	// offset chunk 기록하기
	if (!offsetChunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": LogFont Offset chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	wcout << infoPrefix << L"LogFont/LogFontOffset Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}


bool Archive::rewriteUnknownChunk(HANDLE hNewArchive)
{
	UnknownChunk chunk;
	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 기존 chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}
	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 신규 chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}
	return true;
}




bool Archive::caculateProtectionChecksum(HANDLE hTargetArchive, UINT32& protectionChecksum, bool notNeedToCheckProtectionChunk)
{
	DWORD dRead;
	ChunkInfo chunkInfo;

	bool isAppTargetFileNameChunkExists = false;
	bool isProtectionChunkExists = false;

	UINT32 appTargetFileNameChunkOffset = 0;
	UINT32 protectionChunkOffset = 0;

	// 초기화 및 chunk 시작 위치 지정
	// 대상은 새로 만드는 아카이브이므로 새 아카이브의 chunk 시작 위치를 쓴다 (모듈을 교체했다면 원래 위치와 다를 수 있다)
	memset(&chunkInfo, 0, sizeof(ChunkInfo));
	SetFilePointer(hTargetArchive, newChunkStartOffset, NULL, FILE_BEGIN);

	wcout << infoPrefix << L"Calculating new checksum..." << endl;

	// CHUNK_APP_TARGET_FILENAME, CHUNK_PROTECTION 위치 파악
	while (1) {

		// 다 읽지 못하면 빠져나가기, 값을 체크할 필요가 없을 때를 대비해서 미리 세팅
		UINT32 backupOffset = SetFilePointer(hTargetArchive, 0, NULL, FILE_CURRENT);
		ReadFile(hTargetArchive, &chunkInfo, sizeof(ChunkInfo), &dRead, NULL);
		if (dRead != sizeof(ChunkInfo)) {
			protectionChunkOffset = backupOffset;
			break;
		}

		// protection chunk라도 나간다
		if (chunkInfo.type == ChunkType::CHUNK_APP_TARGET_FILENAME) {
			appTargetFileNameChunkOffset = backupOffset;
			isAppTargetFileNameChunkExists = true;
		}
		if (chunkInfo.type == ChunkType::CHUNK_PROTECTION) {
			protectionChunkOffset = backupOffset;
			isProtectionChunkExists = true;
			break;
		}

		SetFilePointer(hTargetArchive, chunkInfo.size, NULL, FILE_CURRENT);

	}

	// protection chunk를 체크할 필요가 없다면, 즉 이미 protection chunk 이전까지 기록한 상태에서 호출한 거라면
	// isProtectionChunkExists는 참이고, protectionChunkOffset은 현재 hTargetArchive의 위치가 된다
	if (notNeedToCheckProtectionChunk) {
		isProtectionChunkExists = true;
	}

	if (!isAppTargetFileNameChunkExists || !isProtectionChunkExists) {
		return false;
	}

	UINT32 protectionBufferSize = protectionChunkOffset - appTargetFileNameChunkOffset;
	BYTE* protectionBuffer = (BYTE*)malloc(protectionBufferSize);
	SetFilePointer(hTargetArchive, appTargetFileNameChunkOffset, NULL, FILE_BEGIN);
	ReadFile(hTargetArchive, protectionBuffer, protectionBufferSize, &dRead, NULL);

	protectionChecksum = getChecksum(protectionBuffer, protectionBufferSize);
	protectionChecksum ^= 0x0000000D;
	free(protectionBuffer);

	return true;
}


bool Archive::readObjectHeaders(vector<ObjectHeaderEntry>& headers, vector<string>& names)
{
	// 아카이브 핸들의 현재 위치를 백업해 두었다가 마지막에 되돌린다
	DWORD backupOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);
	bool success = false;

	do {
		ObjectHeaderListChunk headerChunk;
		SetFilePointer(hArchive, chunkStartOffset, NULL, FILE_BEGIN);
		if (!moveArchiveHandleToChunk(hArchive, ChunkType::CHUNK_2_5_PLUS_OBJECT_HEADERS) || !headerChunk.readFromArchive(hArchive)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 헤더(0x2253)를 읽어들이지 못했습니다." << endl;
			break;
		}

		ObjectNamesChunk nameChunk;
		SetFilePointer(hArchive, chunkStartOffset, NULL, FILE_BEGIN);
		if (!moveArchiveHandleToChunk(hArchive, ChunkType::CHUNK_2_5_PLUS_OBJECT_NAMES) || !nameChunk.readFromArchive(hArchive)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 이름(0x2254)을 읽어들이지 못했습니다." << endl;
			break;
		}

		headers = headerChunk.getEntries();
		names = nameChunk.getValue();
		success = true;
	} while (false);

	SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
	return success;
}


bool Archive::rewriteObjectElementChunks(HANDLE hNewArchive, vector<wstring>& replacingDataTypes)
{
	bool useText = contains(replacingDataTypes, L"text");
	bool useScript = contains(replacingDataTypes, L"script");

	// 오브젝트 스크립트(JSON) 폴더가 있는지 확인한다
	wstring scriptDirPath = getExtentionRemovedPath(archivePath) + L"/script/object";
	bool scriptDirExists = useScript && (isFileOrDir(scriptDirPath) == FSOType::FSO_DIRECTORY);

	// 오브젝트 텍스트 번역 표는 처음 필요할 때 한 번만 읽어 들인다
	if (useText && !objectTextTable.isLoaded()) {
		wstring importJsonPath = getExtentionRemovedPath(archivePath) + L"/text/objectText.json";
		if (isFileOrDir(importJsonPath) != FSOType::FSO_FILE) {
			wcout << infoPrefix << L"오브젝트 텍스트 json 이 없어 오브젝트 텍스트 교체를 건너뜁니다: " << importJsonPath << endl;
			objectTextTable.markLoaded();
		}
		else {
			Json::Value textValue;
			if (!readFromJson(textValue, importJsonPath) || !objectTextTable.importJson(textValue)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 텍스트 json 을 읽어들이지 못했습니다." << endl;
				triggerError(__FUNCTION__, u8"오브젝트 텍스트 json 을 읽어들이지 못했습니다", true);
				return false;
			}
			wcout << infoPrefix << L"오브젝트 텍스트 " << dec << objectTextTable.getEntryCount() << L"개 오브젝트를 읽어들였습니다." << endl;
		}
	}
	bool hasTextTable = useText && (objectTextTable.getEntryCount() > 0);

	// 적용할 것이 없으면 원본 청크를 그대로 옮긴다
	if (!hasTextTable && !scriptDirExists) {
		if (useScript) {
			wcout << infoPrefix << L"오브젝트 스크립트 폴더가 없어 오브젝트 스크립트 교체를 건너뜁니다: " << scriptDirPath << endl;
		}
		return rewriteUnknownChunk(hNewArchive);
	}

	ObjectElementDataChunk chunk;

	wcout << L"--------------------------------------" << endl;
	wcout << infoPrefix << L"Object Data Chunk rewriting processing..." << endl;

	if (!chunk.readFromArchive(hArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 속성 데이터 chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	vector<ObjectHeaderEntry> headers;
	vector<string> names;
	if (!readObjectHeaders(headers, names)) {
		return false;
	}

	vector<ObjectContainerElement>& elements = chunk.getContainer().getElements();
	if (headers.size() != elements.size()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 헤더 수와 속성 데이터 수가 다릅니다." << endl;
		return false;
	}

	// 번역이 있거나 스크립트 파일이 있는 오브젝트만 해석하여 교체한다
	UINT32 replacedTotal = 0;
	UINT32 skippedTotal = 0;
	UINT32 scriptedTotal = 0;
	for (size_t i = 0; i < elements.size(); i++) {
		const ObjectTextEntry* entry = hasTextTable ? objectTextTable.findEntry((INT32)i) : NULL;

		wostringstream indexStream;
		indexStream << setw(6) << setfill(L'0') << i;
		wstring scriptPath = scriptDirPath + L"/" + indexStream.str() + L".json";
		bool hasScript = scriptDirExists && (isFileOrDir(scriptPath) == FSOType::FSO_FILE);

		if (((entry == NULL) && !hasScript) || (headers[i].type < 2)) {
			continue;
		}

		ObjectCommon common;
		string errorMessage;
		if (!common.parse(elements[i].data, ObjectCommonLayout::TWO_FIVE_PLUS, getBuildVersion(), errorMessage)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 " << dec << i << L" 번을 해석하지 못해 건너뜁니다." << endl;
			continue;
		}

		// 스크립트(JSON)를 먼저 반영하고, 그 위에 텍스트 번역을 적용한다
		bool changedByScript = false;
		if (hasScript) {
			Json::Value scriptValue;
			if (!readFromJson(scriptValue, scriptPath)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << scriptPath << L": 오브젝트 스크립트 json 을 읽어들이지 못했습니다." << endl;
				triggerError(__FUNCTION__, u8"오브젝트 스크립트 json 을 읽어들이지 못했습니다", true);
				return false;
			}
			if (!recompileObjectScript(common, scriptValue, changedByScript, errorMessage)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << scriptPath << L": " << wstring(errorMessage.begin(), errorMessage.end()) << endl;
				triggerError(__FUNCTION__, u8"오브젝트 스크립트를 적용하지 못했습니다", true);
				return false;
			}
		}
		UINT32 replacedCount = 0;
		UINT32 skippedCount = 0;
		if (entry != NULL) {
			replaceObjectText(common, *entry, replacedCount, skippedCount);
		}
		skippedTotal += skippedCount;

		if (!changedByScript && (replacedCount == 0)) {
			continue;
		}

		ByteBuffer blob;
		if (!common.serialize(blob, errorMessage)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 " << dec << i << L" 번을 다시 만들지 못했습니다: " << wstring(errorMessage.begin(), errorMessage.end()) << endl;
			return false;
		}
		chunk.getContainer().setElementData(i, blob);
		replacedTotal += replacedCount;
		if (changedByScript) {
			scriptedTotal++;
		}
	}

	if (!chunk.writeIntoArchive(hNewArchive)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 속성 데이터 chunk를 기록하지 못해 변환을 실패했습니다." << endl;
		return false;
	}

	// 이어지는 오프셋 chunk(0x2257)는 새로 계산한 오프셋으로 다시 쓴다
	ChunkInfo nextInfo;
	DWORD dRead = 0;
	DWORD nextOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);
	ReadFile(hArchive, &nextInfo, sizeof(ChunkInfo), &dRead, NULL);
	SetFilePointer(hArchive, nextOffset, NULL, FILE_BEGIN);
	if ((dRead == sizeof(ChunkInfo)) && (nextInfo.type == ChunkType::CHUNK_2_5_PLUS_OBJECT_OFFSETS)) {
		ObjectOffsetsChunk offsetChunk;
		if (!offsetChunk.readFromArchive(hArchive)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 오프셋 chunk를 읽어들이지 못해 변환을 실패했습니다." << endl;
			return false;
		}
		const ByteBuffer& offsetPayload = chunk.getOffsetPayload();
		vector<UINT32> offsetList(offsetPayload.size() / sizeof(UINT32));
		if (!offsetList.empty()) {
			memcpy(offsetList.data(), &offsetPayload[0], offsetList.size() * sizeof(UINT32));
		}
		offsetChunk.setValue(offsetList);
		if (!offsetChunk.writeIntoArchive(hNewArchive)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 오프셋 chunk를 기록하지 못해 변환을 실패했습니다." << endl;
			return false;
		}
	}

	wcout << infoPrefix << L"오브젝트 스크립트 " << dec << scriptedTotal << L"개를 반영하고, 오브젝트 텍스트 " << replacedTotal << L"개를 교체했습니다. (원문이 달라 건너뜀: " << skippedTotal << L"개)" << endl;
	wcout << infoPrefix << L"Object Data Chunk rewriting complete!" << endl;
	wcout << L"--------------------------------------" << endl << endl;

	return true;
}