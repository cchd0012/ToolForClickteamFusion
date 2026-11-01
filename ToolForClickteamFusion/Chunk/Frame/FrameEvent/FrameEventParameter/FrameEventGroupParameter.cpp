#include "FrameEventGroupParameter.h"
#include "../../../../Archive/Archive.h"
#include "../../../../Util/CommonUtil.h"
#include "../../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameEventGroupParameter::FrameEventGroupParameter()
{
	totalDataBufferSize = 0;
	type = FrameEventParameterType::UNKNOWN;
	id = 0;
	flags = 0;
}


FrameEventGroupParameter::~FrameEventGroupParameter()
{
}


// 버퍼로부터 읽기, 오프셋 매핑하여 저장
bool FrameEventGroupParameter::readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameDataOffsetMapper)
{
	// 끝부분 확인 후 포인터 옮겨주기
	totalDataBufferSize = (*(INT16*)bufferPnt);
	BYTE* endBufferPnt = bufferPnt + totalDataBufferSize;
	bufferPnt += sizeof(INT16);

	// 타입 기록
	type = *(FrameEventParameterType*)bufferPnt;
	bufferPnt += sizeof(FrameEventParameterType);

	// *** 진행중 *** //

	bufferPnt = endBufferPnt;
	return true;
}


// 버퍼에 기록
bool FrameEventGroupParameter::writeIntoBuffer(BYTE*& bufferPnt)
{

	//bufferPnt += sizeof(UINT32);

	return true;
}


// 프레임 이벤트 계열 클래스를 가리키도록 지정
void FrameEventGroupParameter::setPointingFrameEventData(map<INT32, FrameEventData*>& frameDataOffsetMapper)
{
}


// FrameEventData 클래스 지정 (json과 호환 처리)
void FrameEventGroupParameter::setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList)
{
}


// 크기 재계산 후 반환
UINT32 FrameEventGroupParameter::recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList)
{
	totalDataBufferSize = sizeof(INT16) + sizeof(FrameEventParameterType);

	// *** 진행중 *** //

	return totalDataBufferSize;
}


// 오프셋 업데이트
void FrameEventGroupParameter::updateOffset(INT32& accumulatingOffset)
{
	INT32 afterAccumulatingOffset = accumulatingOffset + totalDataBufferSize;
	accumulatingOffset = afterAccumulatingOffset;
}


// 오프셋 보정
void FrameEventGroupParameter::recorrectOffset()
{
}


// json으로 텍스트 추출
bool FrameEventGroupParameter::extractText(Json::Value& extractValue)
{
	return true;
}


// json으로부터 텍스트 교체
bool FrameEventGroupParameter::replaceText(Json::Value& replaceValue, UINT32& replaceIndex)
{
	return true;
}


// json으로 디컴파일
bool FrameEventGroupParameter::decompile(Json::Value& decompileValue)
{
	decompileValue["__type__"] = "group";
	return true;
}


// json으로부터 리컴파일
bool FrameEventGroupParameter::recompile(Json::Value& recompileValue)
{
	return true;
}