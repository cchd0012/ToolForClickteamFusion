#include"FrameEventHeader.h"
#include "../../../Archive/Archive.h"
#include "../../../Util/CommonUtil.h"
#include "../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameEventHeader::FrameEventHeader()
{
	maxObjects = 0;
	maxObjectInfos = 0;
	playerCount = 0;
	conditions.clear();
	qualifiers.clear();
}


FrameEventHeader::~FrameEventHeader()
{
	conditions.clear();
	qualifiers.clear();
}


// 버퍼로부터 읽기, 오프셋 매핑하여 저장
bool FrameEventHeader::readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
	maxObjects = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	maxObjectInfos = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	playerCount = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	conditions.resize(0x11);
	for (UINT32 i = 0; i < (UINT32)conditions.size(); i++) {
		conditions[i] = *(UINT16*)bufferPnt;
		bufferPnt += sizeof(UINT16);
	}

	UINT32 qualifierCount = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	qualifiers.resize(qualifierCount);
	for (UINT32 i = 0; i < qualifierCount; i++) {
		memcpy(&qualifiers[i], bufferPnt, sizeof(FrameEventQualifier));
		bufferPnt += sizeof(FrameEventQualifier);
	}

	return true;
}


// 버퍼에 기록
bool FrameEventHeader::writeIntoBuffer(BYTE*& bufferPnt)
{

	*(UINT16*)bufferPnt = maxObjects;
	bufferPnt += sizeof(UINT16);

	*(UINT16*)bufferPnt = maxObjectInfos;
	bufferPnt += sizeof(UINT16);

	*(UINT16*)bufferPnt = playerCount;
	bufferPnt += sizeof(UINT16);

	*(UINT16*)bufferPnt = (UINT16)(conditions.size());
	bufferPnt += sizeof(UINT16);

	for (UINT32 i = 0; i < (UINT32)conditions.size(); i++) {
		*(UINT16*)bufferPnt = conditions[i];
		bufferPnt += sizeof(UINT16);
	}

	*(UINT16*)bufferPnt = (UINT16)(qualifiers.size());
	bufferPnt += sizeof(UINT16);

	for (UINT32 i = 0; i < qualifiers.size(); i++) {
		memcpy(bufferPnt, &qualifiers[i], sizeof(FrameEventQualifier));
		bufferPnt += sizeof(FrameEventQualifier);
	}

	return true;
}


// 프레임 이벤트 계열 클래스를 가리키도록 지정
void FrameEventHeader::setPointingFrameEventData(map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
}


// FrameEventData 클래스 지정 (json과 호환 처리)
void FrameEventHeader::setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList)
{
}


// 크기 재계산 후 반환
UINT32 FrameEventHeader::recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList)
{
	UINT32 playerConditionsDataSize = (sizeof(UINT16) + (sizeof(UINT16) * (UINT32)conditions.size()));
	UINT32 qualifierDataSize = (sizeof(UINT16) + (sizeof(FrameEventQualifier) * (UINT32)qualifiers.size()));
	return sizeof(UINT16) + sizeof(UINT16) + playerConditionsDataSize + qualifierDataSize;
}


// 오프셋 업데이트
void FrameEventHeader::updateOffset(INT32& accumulatingOffset)
{
}


// 오프셋 보정
void FrameEventHeader::recorrectOffset()
{
}


// json으로 텍스트 추출
bool FrameEventHeader::extractText(Json::Value &extractValue)
{
	return true;
}


// json으로부터 텍스트 교체
bool FrameEventHeader::replaceText(Json::Value &replaceValue, UINT32 &replaceIndex)
{
	return true;
}


// json으로 디컴파일
bool FrameEventHeader::decompile(Json::Value &decompileValue)
{
	decompileValue["__type__"] = "header";

	decompileValue["max_objects"] = maxObjects;
	decompileValue["max_object_infos"] = maxObjectInfos;
	decompileValue["player_count"] = playerCount;

	Json::Value playerConditionsValue;
	for (UINT32 i = 0; i < (UINT32)conditions.size(); i++) {
		playerConditionsValue.append(conditions[i]);
	}
	decompileValue["player_conditions"] = playerConditionsValue;

	Json::Value qualifiersValue;
	for (UINT32 i = 0; i < (UINT32)qualifiers.size(); i++) {
		Json::Value tpElem;
		tpElem["object_info"] = qualifiers[i].objectInfo;
		tpElem["type"] = qualifiers[i].type;
		qualifiersValue.append(tpElem);
	}
	decompileValue["qualifiers"] = playerConditionsValue;

	return true;
}


// json으로부터 리컴파일
bool FrameEventHeader::recompile(Json::Value &recompileValue)
{

	maxObjects = (UINT16)recompileValue["max_objects"].asUInt();
	maxObjectInfos = (UINT16)recompileValue["max_object_infos"].asUInt();
	playerCount = (UINT16)recompileValue["player_count"].asUInt();

	conditions.clear();
	conditions.resize(0x11);
	for (UINT32 i = 0; i < (UINT32)conditions.size(); i++) {
		conditions[i] = (UINT16)recompileValue["player_conditions"][i].asUInt();
	}

	qualifiers.clear();
	qualifiers.resize(recompileValue["qualifiers"].size());
	for (UINT32 i = 0; i < (UINT32)qualifiers.size(); i++) {
		Json::Value tpElem;
		qualifiers[i].objectInfo = (UINT16)recompileValue["qualifiers"][i]["object_info"].asUInt();
		qualifiers[i].type = (INT16)recompileValue["qualifiers"][i]["type"].asInt();
	}

	return true;
}