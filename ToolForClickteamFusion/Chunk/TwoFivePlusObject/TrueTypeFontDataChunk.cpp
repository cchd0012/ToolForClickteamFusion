#include "TrueTypeFontDataChunk.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


bool TrueTypeFontDataChunk::readOnlyDataCount(HANDLE hArchive)
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

	// 개수 확인. 루프를 직접 돌아야 처리할 수 있음...
	BYTE* orgBufferPnt = orgBuffer;
	while (orgBufferPnt < (orgBuffer + orgSize)) {

		CompressedDataInfo cdInfo;
		memset(&cdInfo, 0, sizeof(CompressedDataInfo));

		cdInfo.orgSize = *(UINT32*)orgBufferPnt;
		orgBufferPnt += sizeof(UINT32);
		cdInfo.compressedSize = *(UINT32*)orgBufferPnt;
		orgBufferPnt += sizeof(UINT32);

		orgBufferPnt += cdInfo.compressedSize;

		totalCount++;
	}

	// totalCount 추가
	getStates()->totalDataCount += totalCount;
	getStates()->chunkDataCount = totalCount;

	free(orgBuffer);
	orgBuffer = NULL;

	return true;
}


bool TrueTypeFontDataChunk::loadFromOrgBuffer()
{
	// 오프셋 관련 데이터 초기화
	offsetList.clear();

	// value 초기화
	for (UINT32 i = 0; i < value.size(); i++) {
		if (value[i] != NULL) {
			delete value[i];
		}
	}
	value.clear();

	BYTE* orgBufferPnt = orgBuffer;
	UINT32 accumIndex = 0;
	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": loading datus from buffer..." << endl;
	while (orgBufferPnt < (orgBuffer + orgSize)) {

		// 오프셋 값 추가
		offsetList.push_back((UINT32)(orgBufferPnt - orgBuffer));

		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"loading " << to_wstring(accumIndex + 1) << L" data from buffer...";
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


bool TrueTypeFontDataChunk::saveIntoOrgBuffer()
{
	// 크기 계산하여 재세팅
	UINT32 accumOffset = 0;
	for (size_t i = 0; i < value.size(); i++) {
		accumOffset += value[i]->getDataSize();
	}
	orgSize = accumOffset;

	// 버퍼 할당 및 초기화
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgBuffer = (BYTE*)malloc(orgSize);
	memset(orgBuffer, 0, orgSize);

	// orgBuffer에 기록
	BYTE* orgBufferPnt = orgBuffer;
	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": saving " << to_wstring(value.size()) << L" data to buffer..." << endl;
	for (size_t i = 0; i < value.size(); i++) {

		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"saving " << to_wstring(i + 1) << L"/" << to_wstring(value.size()) << L" data to buffer...";
		if (!value[i]->saveIntoBuffer(orgBufferPnt)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getNameUni(info.type) << L": 데이터를 저장하지 못했습니다." << endl;
			return false;
		}

	}

	return true;
}