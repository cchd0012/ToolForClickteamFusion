#include "FrameEventSizeInfo.h"
#include "FrameEventDataGroup.h"
#include "../../../Archive/Archive.h"
#include "../../../Util/CommonUtil.h"
#include "../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameEventSizeInfo::FrameEventSizeInfo()
{
	totalFrameEventGroupDataSize = 0;
}


FrameEventSizeInfo::~FrameEventSizeInfo()
{
}


// 버퍼로부터 읽기, 오프셋 매핑하여 저장
bool FrameEventSizeInfo::readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameDataOffsetMapper)
{
	totalFrameEventGroupDataSize = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	return true;
}


// 버퍼에 기록
bool FrameEventSizeInfo::writeIntoBuffer(BYTE*& bufferPnt)
{

	*(UINT32*)bufferPnt = totalFrameEventGroupDataSize;
	bufferPnt += sizeof(UINT32);

	return true;
}


// 프레임 이벤트 계열 클래스를 가리키도록 지정
void FrameEventSizeInfo::setPointingFrameEventData(map<INT32, FrameEventData*>& frameDataOffsetMapper)
{
}


// FrameEventData 클래스 지정 (json과 호환 처리)
void FrameEventSizeInfo::setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList)
{
}


// 크기 재계산 후 반환
UINT32 FrameEventSizeInfo::recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList)
{

	// FrameEventSizeInfo 클래스는 FrameEventDataGroup 클래스의 데이터 크기를 전부 더해서 지정해 줘야 한다
	totalFrameEventGroupDataSize = 0;
	for (UINT32 i = 0; i < frameEventClassList.size(); i++) {
		if (dynamic_cast<FrameEventDataGroup*>(frameEventClassList[i])) {
			FrameEventDataGroup* fegdPtr = reinterpret_cast<FrameEventDataGroup*>(frameEventClassList[i]);
			totalFrameEventGroupDataSize += fegdPtr->getTotalFrameEventDataSize();
		}
	}

	return sizeof(UINT32);
}


// 오프셋 업데이트
void FrameEventSizeInfo::updateOffset(INT32& accumulatingOffset)
{
}


// 오프셋 보정
void FrameEventSizeInfo::recorrectOffset()
{
}


// json으로 텍스트 추출
bool FrameEventSizeInfo::extractText(Json::Value& extractValue)
{
	return true;
}


// json으로부터 텍스트 교체
bool FrameEventSizeInfo::replaceText(Json::Value& replaceValue, UINT32& replaceIndex)
{
	return true;
}


// json으로 디컴파일
bool FrameEventSizeInfo::decompile(Json::Value& decompileValue)
{
	decompileValue["__type__"] = "size_info";
	return true;
}


// json으로부터 리컴파일
bool FrameEventSizeInfo::recompile(Json::Value& recompileValue)
{
	return true;
}