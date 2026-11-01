#include "Chunk.h"
#include "ChunkStructure.h"
#include "../Application/Application.h"
#include "../Application/Dx9Application.h"
#include "../Archive/Archive.h"
#include "../Util/DecryptionUtil.h"
#include "../Util/ZlibUtil.h"
#include "../Util/CommonUtil.h"

// ** allocateChunk 때문에 필요하다 **
#include "AppInfo/Headers.h"
#include "Font/Headers.h"
#include "Frame/Headers.h"
#include "Image/Headers.h"
#include "Literal/Headers.h"
#include "Object/Headers.h"
#include "Sound/Headers.h"
#include "TwoFivePlusObject/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::DecryptionUtil;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;

// 압축/암호화 모드에 따른 처리
// Chunk의 구조 : [원래 데이터]
// Chunk의 구조 (압축) : [원래 크기 4바이트] [zlib 압축 크기 4바이트] [zlib 압축 데이터]
// Chunk의 구조 (암호) : encryped-[원래 데이터]
// Chunk의 구조 (암호+압축) : [원래 크기 4바이트] encryped-[[zlib 압축 크기 4바이트] [zlib 압축 데이터]]
// buildVersion이 284를 넘으면 decrypt하기 전 xor을 추가로 해줘야 한다


Chunk::Chunk() 
{
	memset(&info, 0, sizeof(ChunkInfo));
	info.type = ChunkType::CHUNK_UNKNOWN;
	orgBuffer = NULL;
	chunkRead = false;
}


Chunk::~Chunk()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
}


bool Chunk::readFromArchive(HANDLE hArchive) 
{
	DWORD dRead;

	ReadFile(hArchive, &info, sizeof(ChunkInfo), &dRead, NULL);
	if (dRead != sizeof(ChunkInfo)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터 헤더가 깨졌습니다." 
			<< " (offset: 0x" << SetFilePointer(hArchive, 0, NULL, FILE_CURRENT) << L")" << endl;
		return false;
	}

	// 크기에 따라서 읽는 동작 나누기
	BYTE* rawBuffer = (BYTE*)malloc(info.size);
	if (info.size > SEGMENT_SIZE) {
		if (!segmentedReadFile(hArchive, rawBuffer, info.size)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터를 읽어오지 못했습니다."
				<< " (offset: 0x" << uppercase << hex << SetFilePointer(hArchive, 0, NULL, FILE_CURRENT) << L")" << endl;
			free(rawBuffer);
			return false;
		}
	}
	else {
		ReadFile(hArchive, rawBuffer, info.size, &dRead, NULL);
		if (dRead != info.size) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터를 읽어오지 못했습니다."
				<< " (offset: 0x" << uppercase << hex << SetFilePointer(hArchive, 0, NULL, FILE_CURRENT) << L")" << endl;
			free(rawBuffer);
			return false;
		}
	}

	// rawBuffer, info -> orgBuffer, orgSize 세팅
	if (!importRawData(rawBuffer)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터 import를 실패했습니다."
			<< " (offset: 0x" << uppercase << hex << SetFilePointer(hArchive, 0, NULL, FILE_CURRENT) << L")" << endl;
		free(rawBuffer);
		return false;
	}

	// orgBuffer와 orgSize로부터 데이터 세팅
	return loadFromOrgBuffer();
}

bool Chunk::readFromBufferAndChunkInfo (BYTE*& rawBuffer, ChunkInfo& info)
{
	// chunkInfo 복사
	memcpy(&this->info, &info, sizeof(ChunkInfo));

	// rawBuffer, info -> orgBuffer, orgSize 세팅
	if (!importRawData(rawBuffer)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터를 읽어오지 못했습니다. (buffer)" << endl;
		return false;
	}

	// orgBuffer와 orgSize로부터 데이터 세팅
	return loadFromOrgBuffer();
}


bool Chunk::writeIntoArchive(HANDLE hArchive)
{

	DWORD dWrite;

	// orgBuffer와 orgSize에 기록
	if (!saveIntoOrgBuffer()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터를 버퍼에 기록하지 못했습니다."
			<< " (파일 offset: 0x" << uppercase << hex << SetFilePointer(hArchive, 0, NULL, FILE_CURRENT) << L")" << endl;
		return false;
	}

	// orgBuffer, orgSize -> rawBuffer, info 세팅
	BYTE* rawBuffer = NULL;
	if (!exportRawData(rawBuffer)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터를 파일에 기록하지 못했습니다."
			<< " (파일 offset: 0x" << uppercase << hex << SetFilePointer(hArchive, 0, NULL, FILE_CURRENT) << L")" << endl;
		if (rawBuffer != NULL) {
			free(rawBuffer);
			rawBuffer = NULL;
		}
		return false;
	}

	// 정보 및 가공된 데이터를 핸들에 기록 후 해제, 크기에 따라서 나누기
	WriteFile(hArchive, &info, sizeof(ChunkInfo), &dWrite, NULL);
	if (info.size > SEGMENT_SIZE) {
		segmentedWriteFile(hArchive, rawBuffer, info.size);
	}
	else {
		WriteFile(hArchive, rawBuffer, info.size, &dWrite, NULL);
	}
	free(rawBuffer);

	return true;
}

bool Chunk::writeIntoBufferAndChunkInfo(BYTE*& rawBuffer, ChunkInfo& info)
{
	// orgBuffer와 orgSize에 기록
	if (!saveIntoOrgBuffer()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터를 버퍼에 기록하지 못했습니다. (buffer)" << endl;
		return false;
	}

	// orgBuffer, orgSize -> rawBuffer, info.size 세팅
	if (!exportRawData(rawBuffer)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": chunk 데이터를 기록하지 못했습니다. (buffer)" << endl;
		if (rawBuffer != NULL) {
			free(rawBuffer);
			rawBuffer = NULL;
		}
		return false;
	}

	// info 복사
	memcpy(&info, &this->info, sizeof(ChunkInfo));

	return true;
}


bool Chunk::importRawData(BYTE* &rawBuffer)
{
	// 사전 점검
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}

	if (isCompressed()) {
		if (isEncrypted()) {
			if ((((UINT16)(info.type) & 1) != 0) && (getArchive()->getBuildVersion() > 0x11C)) {
				BYTE xorValue = (BYTE)((UINT16)(info.type) ^ ((UINT16)(info.type) >> 8));
				*(rawBuffer + sizeof(UINT32)) ^= xorValue;
			}
			DecryptionUtil::decrypt((rawBuffer + sizeof(UINT32)), (info.size - sizeof(UINT32)));
		}
		UINT32 originalSize = *(UINT32*)(rawBuffer);
		UINT32 compressedSize = *(UINT32*)(rawBuffer + sizeof(UINT32));
		if (info.size != (compressedSize + (sizeof(UINT32) * 2))) {
			// 나중에 매핑시켜서 출력문자열을 자유롭게 변경해야 할듯
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Chunk: chunk 데이터가 깨졌습니다." << endl;
			return false;
		}
		BYTE* compressedData = (rawBuffer + (sizeof(UINT32) * 2));
		BYTE* originalData = NULL;
		if (!ZlibUtil::decode(compressedData, compressedSize, originalData, originalSize)) {
			// 나중에 매핑시켜서 출력문자열을 자유롭게 변경해야 할듯
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Chunk: chunk 데이터를 디코딩하지 못했습니다." << endl;
			free(originalData);
			return false;
		}
		orgBuffer = originalData;
		orgSize = originalSize;
	}
	else if (isEncrypted()) {
		if ((((UINT16)(info.type) & 1) != 0) && (getArchive()->getBuildVersion() > 0x11C)) {
			BYTE xorValue = (BYTE)((UINT16)(info.type) ^ ((UINT16)(info.type) >> 8));
			*(rawBuffer) ^= xorValue;
		}
		DecryptionUtil::decrypt(rawBuffer, info.size);
		orgSize = info.size;
		orgBuffer = (BYTE*)malloc(orgSize);
		memcpy(orgBuffer, rawBuffer, orgSize);
	}
	else {
		orgSize = info.size;
		orgBuffer = (BYTE*)malloc(orgSize);
		memcpy(orgBuffer, rawBuffer, orgSize);
	}
	chunkRead = true;
	return true;
}


bool Chunk::exportRawData(BYTE*& rawBuffer)
{
	if (isCompressed()) {
		BYTE* compressedData = NULL;
		UINT32 compressedSize = 0;
		if (!ZlibUtil::encode(orgBuffer, orgSize, compressedData, compressedSize)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Chunk: chunk 데이터를 인코딩하지 못했습니다." << endl;
			return false;
		}
		info.size = (sizeof(UINT32) * 2) + compressedSize;
		rawBuffer = (BYTE*)malloc(info.size);
		memset(rawBuffer, 0, info.size);
		*(UINT32*)(rawBuffer) = orgSize;
		*(UINT32*)(rawBuffer + sizeof(UINT32)) = compressedSize;
		memcpy((rawBuffer + (sizeof(UINT32) * 2)), compressedData, compressedSize);
		free(compressedData);
		if (isEncrypted()) {
			DecryptionUtil::encrypt((rawBuffer + sizeof(UINT32)), (info.size - sizeof(UINT32)));
			if ((((UINT16)(info.type) & 1) != 0) && (getArchive()->getBuildVersion() > 0x11C)) {
				BYTE xorValue = (BYTE)((UINT16)(info.type) ^ ((UINT16)(info.type) >> 8));
				*(rawBuffer + sizeof(UINT32)) ^= xorValue;
			}
		}
	}
	else {
		info.size = orgSize;
		rawBuffer = (BYTE*)malloc(info.size);
		memcpy(rawBuffer, orgBuffer, info.size);
		if (isEncrypted()) {
			DecryptionUtil::encrypt(rawBuffer, info.size);
			if ((((UINT16)(info.type) & 1) != 0) && (getArchive()->getBuildVersion() > 0x11C)) {
				BYTE xorValue = (BYTE)((UINT16)(info.type) ^ ((UINT16)(info.type) >> 8));
				*(rawBuffer) ^= xorValue;
			}
		}
	}
	return true;
}


// EndChunk 데이터 렌더링용
void EndChunk::renderData()
{
	ImGui::TextDisabled(u8"--");
}


// 일반적일 때 데이터 렌더링용
void UnknownChunk::renderData()
{
	// 60 바이트 선에서 컷
	stringstream oss;
	UINT32 maxCount = (valueSize > 60) ? 60 : valueSize;
	oss << "[";
	for (UINT32 i = 0; i < maxCount;i++) {
		if (i % 0x10 == 0) {
			oss << "\n  ";
		}
		oss << setw(2) << setfill('0') << hex << uppercase << int(value[i]) << " ";
	}
	if (maxCount != valueSize) {
		oss << "... ";
	}
	oss << "\n";
	oss << "]";
	ImGui::Text(oss.str().c_str());
	oss.str("");
}



// ** chunkType에 따라 chunk를 할당해서 반환해 주는 함수 **
CTT::Chunk::Chunk* CTT::Chunk::allocateChunk(ChunkType chunkType, bool& isAppInfo)
{
	// 기본적으로 app 정보가 아닌 것으로 세팅 후, 필요할 때만 true로 세팅한다
	isAppInfo = false;

	// setApplicationInfo()을 통해서 이미 읽은 값들은 아카이브의 Chunk를 반환해 준다
	switch (chunkType)
	{
		// --------------------
		// app 관련 chunk
		case ChunkType::CHUNK_APP_HEADER:
		{
			isAppInfo = true;
			return getArchive()->getAppHeaderChunk();
		}
		case ChunkType::CHUNK_APP_NAME:
		{
			isAppInfo = true;
			return getArchive()->getAppNameChunk();
		}
		case ChunkType::CHUNK_APP_AUTHOR:
		{
			return new AppAuthorChunk();
		}
		case ChunkType::CHUNK_APP_EXTENTION_PATH:
		{
			return new ExtentionPathChunk();
		}
		// 그냥 오브젝트 프로퍼티 (헤더, 이름, 커먼, unknown, 이펙트 등 전부 포함)
		/*
		case ChunkType::CHUNK_OBJECT_PROPERTIES:
		{
			return L"object properties";
		}
		*/
		case ChunkType::CHUNK_APP_EDITOR_FILENAME:
		{
			isAppInfo = true;
			return getArchive()->getEditorFileNameChunk();
		}
		case ChunkType::CHUNK_APP_TARGET_FILENAME:
		{
			return new TargetFileNameChunk();
		}
		case ChunkType::CHUNK_APP_DOCS:
		{
			return new AppDocChunk();
		}
		case ChunkType::CHUNK_APP_OTHER_EXTENTIONS:
		{
			return new AppOtherExtentionsChunk();
		}
		case ChunkType::CHUNK_APP_GLOBAL_STRINGS:
		{
			return new AppGlobalStringsChunk();
		}
		case ChunkType::CHUNK_IS_DEMO:
		{
			return new IsDemoChunk();
		}
		/*
		case ChunkType::CHUNK_SERIAL_NUMBER:
		{
			return L"serial number";
		}
		*/
		case ChunkType::CHUNK_BINARY_FILE_DATA:
		{
			isAppInfo = true;
			return getArchive()->getBinaryFileDataChunk();
		}
		case ChunkType::CHUNK_ABOUT_TEXT:
		{
			return new AboutTextChunk();
		}
		case ChunkType::CHUNK_COPYRIGHT:
		{
			isAppInfo = true;
			return getArchive()->getCopyrightChunk();
		}
		case ChunkType::CHUNK_EXE_ONLY:
		{
			return new ExeOnlyChunk();
		}
		// --------------------
		case ChunkType::CHUNK_PROTECTION:
		{
			isAppInfo = true;
			return getArchive()->getProtectionChunk();
		}
		case ChunkType::CHUNK_SHADER_LIST:
		{
			isAppInfo = true;
			return getArchive()->getShaderListChunk();
		}
		// --------------------
		// 이 app의 코드페이지
		case ChunkType::CHUNK_APP_CODEPAGE:
		{
			isAppInfo = true;
			return getArchive()->getAppCodepageChunk();
		}
		/*
		case ChunkType::CHUNK_FRAME_OFFSET:
		{
			return L"frame offset";
		}
		*/
		// --------------------
		// 4바이트 정수 2개, 앞의 값은 0, 뒤의 값은 product version
		case ChunkType::CHUNK_PRODUCT_VERSION_INFO:
		{
			isAppInfo = true;
			return getArchive()->getProductVersionInfoChunk();
		}
		// --------------------
		// 정수가 3개 있는 값인거 같은데...
		case ChunkType::CHUNK_PRODUCT_BUILD_VERSION_INFO:
		{
			isAppInfo = true;
			return getArchive()->getProductBuildVersionInfoChunk();
		}
		// --------------------
		// 이 app의 로케일 id
		case ChunkType::CHUNK_APP_LOCALE_ID:
		{
			isAppInfo = true;
			return getArchive()->getAppLocaleIdChunk();
		}
		// --------------------
		// +2.5 버전 오브젝트 관련 chunk들
		/*
		case ChunkType::CHUNK_2_5_PLUS_OBJECT_HEADERS:
		{
			return L"2.5+ object headers";
		}
		*/
		case ChunkType::CHUNK_2_5_PLUS_OBJECT_NAMES:
		{
			return new ObjectNamesChunk();
		}
		/*
		case ChunkType::CHUNK_2_5_PLUS_OBJECT_SHADERS:
		{
			return L"2.5+ object shaders";
		}
		case ChunkType::CHUNK_2_5_PLUS_OBJECT_DATA:
		{
			return L"2.5+ object data";
		}
		*/
		// +2.5 버전 오프젝트의 오프셋을 담은 정보인데, 압축된게 있고 아닌게 있어서 build 버전 좀 확인해봐야 할듯
		case ChunkType::CHUNK_2_5_PLUS_OBJECT_OFFSETS:
		{
			return new ObjectOffsetsChunk();
		}
		// 여기 껴있는거 보니까 이 폰트들은 object 2.5+ 이상에서만 쓰는건가봄...
		case ChunkType::CHUNK_2_5_TTF_INFO_LIST:
		{
			return new TrueTypeFontInfoDataChunk();
		}
		case ChunkType::CHUNK_2_5_TTF_DATA_LIST:
		{
			return new TrueTypeFontDataChunk();
		}
		/*
		case ChunkType::CHUNK_2_5_PLUS_SHADER_DATA:
		{
			return L"2.5+ shader data";
		}
		case ChunkType::CHUNK_2_5_UNKNOWN_1:
		{
			return L"2.5+ (unknown 1)";
		}
		case ChunkType::CHUNK_2_5_UNKNOWN_2:
		{
			return L"2.5+ (unknown 2)";
		}
		*/
		// --------------------
		// 프레임 관련 chun (FrameChunk는 그냥 UnknownChunk로 처리하는 게 편하다)
		case ChunkType::CHUNK_FRAME_HEADER:
		{
			return new FrameHeaderChunk();
		}
		case ChunkType::CHUNK_FRAME_NAME:
		{
			return new FrameNameChunk();
		}
		/*
		case ChunkType::CHUNK_FRAME_PASSWORD:
		{
			return L"frame password";
		}
		*/
		case ChunkType::CHUNK_FRAME_PALETTE:
		{
			return new FramePaletteChunk();
		}
		case ChunkType::CHUNK_FRAME_OBJECT_INSTANCE:
		{
			return new FrameObjectDataChunk();
		}
		case ChunkType::CHUNK_FRAME_FADE_IN_FRAME:
		{
			return new FrameFadeInFrameChunk();
		}
		case ChunkType::CHUNK_FRAME_FADE_OUT_FRAME:
		{
			return new FrameFadeOutFrameChunk();
		}
		case ChunkType::CHUNK_FRAME_FADE_IN:
		{
			return new FrameFadeInChunk();
		}
		case ChunkType::CHUNK_FRAME_FADE_OUT:
		{
			return new FrameFadeOutChunk();
		}
		/*
		case ChunkType::CHUNK_FRAME_EVENTS:
		{
			return new FrameEventChunk();
		}
		*/
		/*
		case ChunkType::CHUNK_FRAME_PLAY_HEADER:
		{
			return L"frame play header";
		}
		case ChunkType::CHUNK_FRAME_ADDITIONAL_ITEM:
		{
			return L"frame additional item";
		}
		case ChunkType::CHUNK_FRAME_ADDITIONAL_ITEM_INSTANCE:
		{
			return L"frame additional item instance";
		}
		*/
		case ChunkType::CHUNK_FRAME_LAYERS:
		{
			return new FrameLayerDataChunk();
		}
		case ChunkType::CHUNK_FRAME_VIRTUAL_RECT:
		{
			return new FrameVirtualRectChunk();
		}
		case ChunkType::CHUNK_FRAME_DEMO_FILE_PATH:
		{
			return new FrameDemoFilePathChunk();
		}
		case ChunkType::CHUNK_FRAME_RANDOM_SEED:
		{
			return new FrameRandomSeedChunk();
		}
		case ChunkType::CHUNK_FRAME_LAYER_EFFECTS:
		{
			return new FrameLayerEffectDataChunk();
		}
		/*
		case ChunkType::CHUNK_FRAME_BLU_LAY_OPTIONS:
		{
			return L"frame blu lay options";
		}
		*/
		case ChunkType::CHUNK_FRAME_MVT_TIMER_BASE:
		{
			return new FrameMvtTimerBaseChunk();
		}
		/*
		case ChunkType::CHUNK_FRAME_MOSIAC_IMAGE_TABLE:
		{
			return L"frame mosiac image table";
		}
		*/
		case ChunkType::CHUNK_FRAME_EFFECTS:
		{
			return new FrameEffectChunk();
		}
		/*
		case ChunkType::CHUNK_FRAME_IPHONE_OPTIONS:
		{
			return L"frame iphone options";
		}
		case ChunkType::CHUNK_FRAME_UNKNOWN_1:
		{
			return L"frame (unknown 1)";
		}
		*/
		case ChunkType::CHUNK_FRAME_INDEX:
		{
			return new FrameIndexChunk();
		}
		// --------------------
		// 그냥 오브젝트 관련 chunk
		/*
		case ChunkType::CHUNK_OBJECT_HEADER:
		{
			return L"object header";
		}
		case ChunkType::CHUNK_OBJECT_NAME:
		{
			return L"object name";
		}
		case ChunkType::CHUNK_OBJECT_COMMON:
		{
			return L"object common";
		}
		case ChunkType::CHUNK_OBJECT_UNKNOWN:
		{
			return L"object unknown";
		}
		case ChunkType::CHUNK_OBJECT_EFFECTS:
		{
			return L"object effects";
		}
		case ChunkType::CHUNK_OBJECT_SHAPES:
		{
			return L"object shapes";
		}
		*/
		// --------------------
		// 오프셋 정보
		case ChunkType::CHUNK_IMAGE_OFFSETS:
		{
			return new ImageOffsetsChunk();
		}
		case ChunkType::CHUNK_LOG_FONT_OFFSETS:
		{
			return new LogFontOffsetsChunk();
		}
		case ChunkType::CHUNK_SOUND_OFFSETS:
		{
			return new SoundOffsetsChunk();
		}
		case ChunkType::CHUNK_MUSIC_OFFSETS:
		{
			return new MusicOffsetsChunk();
		}
		// --------------------
		// 데이터 정보
		case ChunkType::CHUNK_IMAGE_DATA:
		{
			return new ImageDataChunk();
		}
		case ChunkType::CHUNK_LOG_FONT_DATA:
		{
			return new LogFontDataChunk();
		}
		case ChunkType::CHUNK_SOUND_DATA:
		{
			return new SoundDataChunk();
		}
		case ChunkType::CHUNK_MUSIC_DATA:
		{
			// 이건 실제로 있는지도 잘 모르겠음
			return new UnknownChunk();
		}
		// --------------------
		case ChunkType::CHUNK_END:
		{
			return new EndChunk();
		}
		// --------------------
		// 그냥 귀찮은건 UnknownChunk로 땡치자
		default:
		{
			return new UnknownChunk();
		}
	}
}


// ** 해당 chunkType이 위치한 오프셋을 세팅해 주는 함수, 성공하면 true **
bool CTT::Chunk::moveArchiveHandleToChunk(HANDLE hArchive, ChunkType chunkType)
{
	DWORD dRead;
	ChunkInfo chunkInfo;

	memset(&chunkInfo, 0, sizeof(ChunkInfo));

	// chunkType이 EndChunk면 확인 안하고 그냥 false 리턴
	if (chunkType == ChunkType::CHUNK_END) {
		return false;
	}

	while (1) {
		
		// chunk를 읽어들일 때를 대비해 위치 백업 필요
		UINT32 backupOffset = SetFilePointer(hArchive, 0, NULL, FILE_CURRENT);
		ReadFile(hArchive, &chunkInfo, sizeof(ChunkInfo), &dRead, NULL);
		SetFilePointer(hArchive, backupOffset, NULL, FILE_BEGIN);
		
		// 읽어온 길이가 0이라면 그냥 리턴한다
		if (dRead != sizeof(ChunkInfo)) {
			return false;
		}

		// 원하는 위치를 찾았으면 되돌리고 리턴해준다
		if (chunkInfo.type == chunkType) {
			return true;
		}

		// 위치 이동
		SetFilePointer(hArchive, sizeof(ChunkInfo) + chunkInfo.size, NULL, FILE_CURRENT);

	}

	return true;
}