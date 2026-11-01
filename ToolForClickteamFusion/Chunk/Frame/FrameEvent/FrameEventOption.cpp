#include"FrameEventOption.h"
#include "../../../Archive/Archive.h"
#include "../../../Util/CommonUtil.h"
#include "../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameEventOption::FrameEventOption()
{
	optionValue = 0;
}


FrameEventOption::~FrameEventOption()
{
}


// 버퍼로부터 읽기, 오프셋 매핑하여 저장
bool FrameEventOption::readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
	optionValue = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	return true;
}


// 버퍼에 기록
bool FrameEventOption::writeIntoBuffer(BYTE*& bufferPnt)
{

	*(UINT32*)bufferPnt = optionValue;
	bufferPnt += sizeof(UINT32);

	return true;
}


// 프레임 이벤트 계열 클래스를 가리키도록 지정
void FrameEventOption::setPointingFrameEventData(map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
}


// FrameEventData 클래스 지정 (json과 호환 처리)
void FrameEventOption::setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList)
{
}


// 크기 재계산 후 반환
UINT32 FrameEventOption::recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList)
{
	return sizeof(UINT32);
}


// 오프셋 업데이트
void FrameEventOption::updateOffset(INT32& accumulatingOffset)
{
}


// 오프셋 보정
void FrameEventOption::recorrectOffset()
{
}


// json으로 텍스트 추출
bool FrameEventOption::extractText(Json::Value& extractValue)
{
	return true;
}


// json으로부터 텍스트 교체
bool FrameEventOption::replaceText(Json::Value& replaceValue, UINT32& replaceIndex)
{
	return true;
}


// json으로 디컴파일
bool FrameEventOption::decompile(Json::Value& decompileValue)
{
	decompileValue["__type__"] = "option";
	decompileValue["value"] = optionValue;
	return true;
}


// json으로부터 리컴파일
bool FrameEventOption::recompile(Json::Value& recompileValue)
{
	optionValue = recompileValue["value"].asUInt();
	return true;
}