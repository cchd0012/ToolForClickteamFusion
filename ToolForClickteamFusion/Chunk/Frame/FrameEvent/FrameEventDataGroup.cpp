#include "FrameEventDataGroup.h"
#include "FrameEventChunk.h"
#include "../../../Archive/Archive.h"
#include "../../../Util/CommonUtil.h"
#include "../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameEventDataGroup::FrameEventDataGroup()
{
	totalFrameEventDataSize = 0;
	frameEventDataList.clear();
}


FrameEventDataGroup::~FrameEventDataGroup()
{
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size();i++) {
		if (frameEventDataList[i] != NULL) {
			delete frameEventDataList[i];
			frameEventDataList[i] = NULL;
		}
	}
	frameEventDataList.clear();
}


// 버퍼로부터 읽기, 오프셋 매핑하여 저장
bool FrameEventDataGroup::readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
	totalFrameEventDataSize = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	// 끝부분 확인
	BYTE* endBufferPnt = bufferPnt + totalFrameEventDataSize;

	// 프레임 데이터 순서만 초기화
	setFrameEventDataOrder(0);

	// 루프를 돌면서 모든 FrameEventData를 읽어들인다
	while (bufferPnt < endBufferPnt) {
		// FrameEventData 읽어들이기
		FrameEventData *data = new FrameEventData();
		if (!data->readFromBuffer(bufferPnt, baseBuffer, frameEventDataOffsetMapper)) {
			delete data;
			return false;
		}
		frameEventDataList.push_back(data);
		// 이후 프레임 데이터 순서 늘려주기
		increaseFrameEventDataOrder();
	}

	// 이후 프레임 데이터 그룹 순서 늘려주고 반환
	increaseFrameEventDataGroupOrder();

	return true;
}


// 버퍼에 기록
bool FrameEventDataGroup::writeIntoBuffer(BYTE*& bufferPnt)
{
	*(UINT32*)bufferPnt = totalFrameEventDataSize;
	bufferPnt += sizeof(UINT32);

	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		frameEventDataList[i]->writeIntoBuffer(bufferPnt);
	}

	return true;
}


// 프레임 이벤트 계열 클래스를 가리키도록 지정
void FrameEventDataGroup::setPointingFrameEventData(map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size();i++) {
		frameEventDataList[i]->setPointingFrameEventData(frameEventDataOffsetMapper);
	}
}


// FrameEventData 클래스 지정 (json과 호환 처리)
void FrameEventDataGroup::setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList)
{
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		frameEventDataList[i]->setPointingFrameEventData(frameEventDataGroupList);
	}
}


// 크기 재계산 후 반환
UINT32 FrameEventDataGroup::recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList)
{
	totalFrameEventDataSize = 0;
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		totalFrameEventDataSize += frameEventDataList[i]->recalculateSize(frameEventClassList);
	}
	return (sizeof(UINT32) + totalFrameEventDataSize);
}


// 오프셋 업데이트
void FrameEventDataGroup::updateOffset(INT32& accumulatingOffset)
{
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		frameEventDataList[i]->updateOffset(accumulatingOffset);
	}
}


// 오프셋 보정
void FrameEventDataGroup::recorrectOffset()
{
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		frameEventDataList[i]->recorrectOffset();
	}
}


// json으로 텍스트 추출
bool FrameEventDataGroup::extractText(Json::Value& extractValue)
{
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		if (!frameEventDataList[i]->extractText(extractValue)) {
			return false;
		}
	}
	return true;
}


// json으로부터 텍스트 교체
bool FrameEventDataGroup::replaceText(Json::Value& replaceValue, UINT32& replaceIndex)
{
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		if (!frameEventDataList[i]->replaceText(replaceValue, replaceIndex)) {
			return false;
		}
	}
	return true;
}


// json으로 디컴파일
bool FrameEventDataGroup::decompile(Json::Value& decompileValue)
{
	decompileValue["__type__"] = "data_group";

	Json::Value frameEventDataListValue;
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		Json::Value eachDataValue;
		if (!frameEventDataList[i]->decompile(eachDataValue)) {
			return false;
		}
		frameEventDataListValue.append(eachDataValue);
	}
	decompileValue["data_list"] = frameEventDataListValue;
	return true;
}


// json으로부터 리컴파일
bool FrameEventDataGroup::recompile(Json::Value& recompileValue)
{
	// 깔끔하게 제거
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		if (frameEventDataList[i] != NULL) {
			delete frameEventDataList[i];
		}
	}
	frameEventDataList.clear();

	// 이후 json으로부터 재생성
	frameEventDataList.resize(recompileValue["data_list"].size());
	for (UINT32 i = 0; i < (UINT32)frameEventDataList.size(); i++) {
		FrameEventData* data = new FrameEventData();
		if (!data->recompile(recompileValue["data_list"][i])) {
			delete data;
			return false;
		}
		frameEventDataList[i] = data;
	}

	return true;
}