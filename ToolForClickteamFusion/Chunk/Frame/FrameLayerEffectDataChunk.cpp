#include "FrameLayerEffectDataChunk.h"
#include "FrameLayerEffect.h"
#include "../../Archive/Archive.h"
#include "../../Util/JsonUtil.h"
#include "../../Util/CommonUtil.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;


bool FrameLayerEffectDataChunk::loadFromOrgBuffer()
{
	BYTE* orgBufferPnt = orgBuffer;

	// 기존 DataChunk와는 다르게 개수를 직접 읽어들이지 않으므로 직접 읽는 수밖에 없다
	// FrameLayerDataChunk 내부의 FrameLayer 리스트와는 일대일 대응이므로 개수를 미리 입력받을 것을 전제한다
	// 내부적으로는 이게 끝인지 파악이 안됨
	UINT32 totalCount = frameLayerCount;

	// value 초기화
	for (UINT32 i = 0; i < value.size(); i++) {
		if (value[i] != NULL) {
			delete value[i];
		}
	}
	value.clear();

	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": loading " << to_wstring(totalCount) << L" data from buffer..." << endl;
	for (UINT32 i = 0; i < totalCount;i++) {

		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"loading " << to_wstring(i + 1) << L"/" << to_wstring(totalCount) << L" data from buffer...";

		BaseData* data = allocateData();

		// 각 FrameLayerEffect의 pamameter 정보가 있는 오프셋은 orgBuffer 시작점이 기준이므로
		// loadFromBuffer 이전에 orgBuffer의 시작점을 세팅해줘야 함
		FrameLayerEffect* flePtr = reinterpret_cast<FrameLayerEffect*>(data);
		flePtr->setBaseBuffer(orgBuffer);

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


bool FrameLayerEffectDataChunk::saveIntoOrgBuffer()
{
	// 누적크기 계산 및 오프셋 세팅
	// 전체 크기 계산 자체는 FrameEffectInfo + (UINT32 * 2) + info 크기지만
	// param을 기록하는 오프셋은 (FrameEffectInfo + (UINT32 * 2))만 더한 크기로 계산해야 한다

	UINT32 accumOffset = 0;
	for (size_t i = 0; i < value.size(); i++) {
		accumOffset += value[i]->getDataSize();
	}
	orgSize = accumOffset;

	// 버퍼 할당
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgBuffer = (BYTE*)malloc(orgSize);
	memset(orgBuffer, 0, orgSize);

	// orgBuffer에 기록, 기록을 마친 데이터는 정리
	BYTE* orgBufferPnt = orgBuffer;

	// valuesBuffer 세팅
	BYTE* valuesBufferPnt = orgBuffer + ((sizeof(FrameEffectInfo) + (sizeof(UINT32) * 2)) * value.size());

	wcout << infoPrefix << L"ChunkType: " << uppercase << hex << getNameUni(info.type) << L": saving " << to_wstring(value.size()) << L" data to buffer..." << endl;
	for (size_t i = 0; i < value.size(); i++) {
		//wcout << flush << L"\r";
		//wcout << infoPrefix << L"saving " << to_wstring(i + 1) << L"/" << to_wstring(value.size()) << L" data to buffer...";
		
		// 버퍼 포인터 세팅
		FrameLayerEffect* flePtr = reinterpret_cast<FrameLayerEffect*>(value[i]);
		flePtr->setBaseBuffer(orgBuffer);
		flePtr->setValuesBufferPnt(valuesBufferPnt);
		if (!flePtr->saveIntoBuffer(orgBufferPnt)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getNameUni(info.type) << L": 데이터를 저장하지 못했습니다." << endl;
			return false;
		}

		// valuesBufferPnt 증가시키기
		valuesBufferPnt += flePtr->getValuesSize();

	}

	return true;
}