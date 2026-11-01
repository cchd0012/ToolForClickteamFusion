#include "FrameEventDataPointerParameter.h"
#include "../FrameEventData.h"
#include "../FrameEventDataGroup.h"
#include "../../../../Archive/Archive.h"
#include "../../../../Util/CommonUtil.h"
#include "../../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameEventDataPointerParameter::FrameEventDataPointerParameter()
{
	basisOffset = 0;
	totalDataBufferSize = 0;
	type = FrameEventParameterType::UNKNOWN;
	movingDistance = 0;
	pointingOffset = 0;
	pointingGroupOrder = -1;
	pointingDataOrder = -1;
	pointingFrameEventData = NULL;
	id = 0;
}


FrameEventDataPointerParameter::~FrameEventDataPointerParameter()
{
}


// 버퍼로부터 읽기, 오프셋 매핑하여 저장
bool FrameEventDataPointerParameter::readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameDataOffsetMapper)
{
	// 기준이 될 오프셋 저장
	basisOffset = (INT32)(bufferPnt - baseBuffer);

	// 끝부분 확인 후 포인터 옮겨주기
	totalDataBufferSize = (*(INT16*)bufferPnt);
	BYTE* endBufferPnt = bufferPnt + totalDataBufferSize;
	bufferPnt += sizeof(INT16);

	// 타입 기록
	type = *(FrameEventParameterType*)bufferPnt;
	bufferPnt += sizeof(FrameEventParameterType);

	movingDistance = *(INT32*)bufferPnt;
	bufferPnt += sizeof(INT32);
	pointingOffset = basisOffset + movingDistance;

	id = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	bufferPnt = endBufferPnt;
	return true;
}


// 버퍼에 기록
bool FrameEventDataPointerParameter::writeIntoBuffer(BYTE*& bufferPnt)
{
	*(UINT16*)bufferPnt = totalDataBufferSize;
	bufferPnt += sizeof(UINT16);

	*(FrameEventParameterType*)bufferPnt = type;
	bufferPnt += sizeof(FrameEventParameterType);

	*(INT32*)bufferPnt = movingDistance;
	bufferPnt += sizeof(UINT32);

	*(UINT32*)bufferPnt = id;
	bufferPnt += sizeof(UINT32);

	return true;
}


// 프레임 이벤트 계열 클래스를 가리키도록 지정
void FrameEventDataPointerParameter::setPointingFrameEventData(map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
	pointingFrameEventData = frameEventDataOffsetMapper[pointingOffset];
}


// FrameEventData 클래스 지정 (json과 호환 처리)
void FrameEventDataPointerParameter::setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList)
{
	vector<FrameEventData*>& frameEventDataList = frameEventDataGroupList[pointingGroupOrder]->getFrameEventDataList();
	pointingFrameEventData = frameEventDataList[pointingDataOrder];
}


// 크기 재계산 후 반환
UINT32 FrameEventDataPointerParameter::recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList)
{
	totalDataBufferSize = sizeof(INT16) + sizeof(FrameEventParameterType);

	totalDataBufferSize += sizeof(INT32);
	totalDataBufferSize += sizeof(UINT32);

	return totalDataBufferSize;
}


// 오프셋 업데이트
void FrameEventDataPointerParameter::updateOffset(INT32& accumulatingOffset)
{
	basisOffset = accumulatingOffset;
	INT32 afterAccumulatingOffset = basisOffset + totalDataBufferSize;
	accumulatingOffset = afterAccumulatingOffset;
}


// 오프셋 보정
void FrameEventDataPointerParameter::recorrectOffset()
{
	pointingOffset = pointingFrameEventData->getOffset();
	movingDistance = pointingOffset - basisOffset;
}


// json으로 텍스트 추출
bool FrameEventDataPointerParameter::extractText(Json::Value& extractValue)
{
	return true;
}


// json으로부터 텍스트 교체
bool FrameEventDataPointerParameter::replaceText(Json::Value& replaceValue, UINT32& replaceIndex)
{
	return true;
}


// json으로 디컴파일
bool FrameEventDataPointerParameter::decompile(Json::Value& decompileValue)
{
	decompileValue["__type__"] = "data_pointer";
	return true;
}


// json으로부터 리컴파일
bool FrameEventDataPointerParameter::recompile(Json::Value& recompileValue)
{
	return true;
}