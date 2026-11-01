#include "TrueTypeFontInfoDataChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/JsonUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;


bool TrueTypeFontInfoDataChunk::readOnlyDataCount(HANDLE hArchive)
{
	DWORD dRead;

	// 정확한 개수를 알기 위해서는 직접 읽어들여야 한다

	UINT32 totalCount = 0;
	ReadFile(hArchive, &info, sizeof(ChunkInfo), &dRead, NULL);

	BYTE* rawBuffer = (BYTE*)malloc(info.size);
	if (info.size > SEGMENT_SIZE) {
		segmentedReadFile(hArchive, rawBuffer, info.size);
	}
	else {
		ReadFile(hArchive, rawBuffer, info.size, &dRead, NULL);
	}
	importRawData(rawBuffer);
	free(rawBuffer);

	// 개수 확인
	totalCount = orgSize / sizeof(CommonTrueTypeFontInfo);

	// totalCount 추가
	getStates()->totalDataCount += totalCount;
	getStates()->chunkDataCount = totalCount;

	free(orgBuffer);
	orgBuffer = NULL;

	return true;
}


bool TrueTypeFontInfoDataChunk::loadFromOrgBuffer()
{
	BYTE* orgBufferPnt = orgBuffer;

	UINT32 totalCount = orgSize / sizeof(CommonTrueTypeFontInfo);

	// value 초기화
	for (UINT32 i = 0; i < value.size(); i++) {
		if (value[i] != NULL) {
			delete value[i];
		}
	}
	value.clear();

	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": loading " << to_wstring(totalCount) << L" data from buffer..." << endl;
	for (UINT32 i = 0; i < totalCount; i++) {

		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"loading " << to_wstring(i + 1) << L"/" << to_wstring(totalCount) << L" data from buffer...";
		BaseData* data = allocateData();
		if (!data->loadFromBuffer(orgBufferPnt)) {
			delete data;
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getNameUni(info.type) << L": 데이터를 로드하지 못했습니다." << endl;
			return false;
		}
		value.push_back(data);

	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool TrueTypeFontInfoDataChunk::saveIntoOrgBuffer()
{
	// 크기 계산하여 재세팅
	orgSize = (UINT32)value.size() * sizeof(CommonTrueTypeFontInfo);

	// 버퍼 할당 및 초기화
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgBuffer = (BYTE*)malloc(orgSize);
	memset(orgBuffer, 0, orgSize);

	// orgBuffer에 기록
	BYTE* orgBufferPnt = orgBuffer;
	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": saving " << to_wstring(value.size()) << L" data to buffer...";
	for (size_t i = 0; i < value.size(); i++) {
		// NULL 대비 코드
		if (value[i] == NULL) {
			continue;
		}
		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"saving " << to_wstring(i + 1) << L"/" << to_wstring(value.size()) << L" data to buffer...";
		if (!value[i]->saveIntoBuffer(orgBufferPnt)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getNameUni(info.type) << L": 데이터를 저장하지 못했습니다." << endl;
			return false;
		}
	}

	return true;
}