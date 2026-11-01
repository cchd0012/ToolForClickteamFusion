#include "FrameEventData.h"
#include "FrameEventDataGroup.h"
#include "FrameEventChunk.h"
#include "../../../Archive/Archive.h"
#include "../../../Util/CommonUtil.h"
#include "../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameEventData::FrameEventData()
{
	basisOffset = 0;
	totalDataBufferSize = 0;
	flags = 0;
	identifier = 0;
	isRestricted = 0;
	restrictCpt = 0;
	undo = 0;
	conditions.clear();
	actions.clear();
	groupOrder = -1;
	dataOrder = -1;
}


FrameEventData::~FrameEventData()
{
	for (UINT32 i = 0; i < (UINT32)conditions.size(); i++) {
		if (conditions[i] != NULL) {
			delete conditions[i];
			conditions[i] = NULL;
		}
	}
	conditions.clear();

	for (UINT32 i = 0; i < (UINT32)actions.size(); i++) {
		if (actions[i] != NULL) {
			delete actions[i];
			actions[i] = NULL;
		}
	}
	actions.clear();
}


// 버퍼로부터 읽기, 오프셋 매핑하여 저장
bool FrameEventData::readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
	// basisOffset 지정 후 매퍼에 추가
	basisOffset = (INT32)(bufferPnt - baseBuffer);
	frameEventDataOffsetMapper.insert({ basisOffset, this });

	// 순서 지정
	groupOrder = getFrameEventDataGroupOrder();
	dataOrder = getFrameEventDataOrder();

	// 끝부분 확인 후 포인터 옮겨주기
	totalDataBufferSize = -(*(INT16*)bufferPnt);
	BYTE* endBufferPnt = bufferPnt + totalDataBufferSize;
	bufferPnt += sizeof(INT16);

	UINT32 conditionCount = *(bufferPnt);
	bufferPnt++;

	UINT32 actionCount = *(bufferPnt);
	bufferPnt++;

	flags = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	identifier = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	// 버전에 따라 구분
	if (getArchive()->getBuildVersion() > 0x11C) {
		// 신버전
		isRestricted = *(UINT32*)bufferPnt;
		bufferPnt += sizeof(UINT32);

		restrictCpt = *(UINT32*)bufferPnt;
		bufferPnt += sizeof(UINT32);
	}
	else {
		// 구버전
		isRestricted = *(UINT16*)bufferPnt;
		bufferPnt += sizeof(UINT16);

		restrictCpt = *(UINT16*)bufferPnt;
		bufferPnt += sizeof(UINT16);

		undo = *(UINT16*)bufferPnt;
		bufferPnt += sizeof(UINT16);
	}

	// condition 데이터 읽어들이기
	conditions.resize(conditionCount);
	for (UINT32 i = 0; i < conditionCount;i++) {
		FrameEventCondition* condition = new FrameEventCondition();
		if (!condition->readFromBuffer(bufferPnt, baseBuffer, frameEventDataOffsetMapper)) {
			delete condition;
			return false;
		}
		conditions[i] = condition;
	}

	// action 데이터 읽어들이기
	actions.resize(actionCount);
	for (UINT32 i = 0; i < actionCount; i++) {
		FrameEventAction* action = new FrameEventAction();
		if (!action->readFromBuffer(bufferPnt, baseBuffer, frameEventDataOffsetMapper)) {
			delete action;
			return false;
		}
		actions[i] = action;
	}

	// 위치 수정 후 반환
	bufferPnt = endBufferPnt;
	return true;
}


// 버퍼에 기록
bool FrameEventData::writeIntoBuffer(BYTE*& bufferPnt)
{

	*(INT16*)bufferPnt = -totalDataBufferSize;
	bufferPnt += sizeof(INT16);

	*(bufferPnt) = (BYTE)(conditions.size());
	bufferPnt++;

	*(bufferPnt) = (BYTE)(actions.size());
	bufferPnt++;

	*(UINT16*)bufferPnt = flags;
	bufferPnt += sizeof(UINT16);

	*(UINT16*)bufferPnt = identifier;
	bufferPnt += sizeof(UINT16);

	// 버전에 따라 구분
	if (getArchive()->getBuildVersion() > 0x11C) {
		// 신버전
		*(UINT32*)bufferPnt = isRestricted;
		bufferPnt += sizeof(UINT32);

		*(UINT32*)bufferPnt = restrictCpt;
		bufferPnt += sizeof(UINT32);
	}
	else {
		// 구버전
		*(UINT16*)bufferPnt = (UINT16)isRestricted;
		bufferPnt += sizeof(UINT16);

		*(UINT16*)bufferPnt = (UINT16)restrictCpt;
		bufferPnt += sizeof(UINT16);

		*(UINT16*)bufferPnt = undo;
		bufferPnt += sizeof(UINT16);
	}

	// condition 데이터 기록
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		FrameEventCondition* condition = new FrameEventCondition();
		if (!condition->writeIntoBuffer(bufferPnt)) {
			delete condition;
			return false;
		}
		conditions[i] = condition;
	}

	// action 데이터 기록
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		FrameEventAction* action = new FrameEventAction();
		if (!action->writeIntoBuffer(bufferPnt)) {
			delete action;
			return false;
		}
		actions[i] = action;
	}

	return true;
}


// 프레임 이벤트 계열 클래스를 가리키도록 지정
void FrameEventData::setPointingFrameEventData(map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
	// condition
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		conditions[i]->setPointingFrameEventData(frameEventDataOffsetMapper);
	}
	// action
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		actions[i]->setPointingFrameEventData(frameEventDataOffsetMapper);
	}
}


// FrameEventData 클래스 지정 (json과 호환 처리)
void FrameEventData::setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList)
{
	// condition
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		conditions[i]->setPointingFrameEventData(frameEventDataGroupList);
	}
	// action
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		actions[i]->setPointingFrameEventData(frameEventDataGroupList);
	}
}


// 크기 재계산 후 반환
UINT32 FrameEventData::recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList)
{
	totalDataBufferSize = sizeof(UINT16);

	totalDataBufferSize++;
	totalDataBufferSize++;
	totalDataBufferSize += sizeof(UINT16);
	totalDataBufferSize += sizeof(UINT16);

	// 버전에 따라 구분
	if (getArchive()->getBuildVersion() > 0x11C) {
		// 신버전
		totalDataBufferSize += sizeof(UINT32);
		totalDataBufferSize += sizeof(UINT32);
	}
	else {
		// 구버전
		totalDataBufferSize += sizeof(UINT16);
		totalDataBufferSize += sizeof(UINT16);
		totalDataBufferSize += sizeof(UINT16);
	}

	// condition
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		totalDataBufferSize += conditions[i]->recalculateSize(frameEventClassList);
	}

	// action
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		totalDataBufferSize += actions[i]->recalculateSize(frameEventClassList);
	}

	return totalDataBufferSize;
}


// 오프셋 업데이트
void FrameEventData::updateOffset(INT32& accumulatingOffset)
{
	// basisOffset 및 작업 후의 위치 지정
	basisOffset = accumulatingOffset;
	INT32 afterAccumulatingOffset = basisOffset + totalDataBufferSize;

	// condition
	for (UINT32 i = 0; i < (UINT32)conditions.size(); i++) {
		conditions[i]->updateOffset(accumulatingOffset);
	}
	// action
	for (UINT32 i = 0; i < (UINT32)actions.size(); i++) {
		actions[i]->updateOffset(accumulatingOffset);
	}

	// 이후 위치 변동시키고 리턴
	accumulatingOffset = afterAccumulatingOffset;
}


// 오프셋 보정
void FrameEventData::recorrectOffset()
{
	// condition
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		conditions[i]->recorrectOffset();
	}

	// action
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		actions[i]->recorrectOffset();
	}
}


// json으로 텍스트 추출
bool FrameEventData::extractText(Json::Value& extractValue)
{
	// condition
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		if (!conditions[i]->extractText(extractValue)) { return false; }
	}

	// action
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		if (!actions[i]->extractText(extractValue)) { return false; }
	}

	return true;
}


// json으로부터 텍스트 교체
bool FrameEventData::replaceText(Json::Value& replaceValue, UINT32& replaceIndex)
{
	// condition
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		if (!conditions[i]->replaceText(replaceValue, replaceIndex)) { return false; }
	}

	// action
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		if (!actions[i]->replaceText(replaceValue, replaceIndex)) { return false; }
	}

	return true;
}


// json으로 디컴파일
bool FrameEventData::decompile(Json::Value& decompileValue)
{
	decompileValue["__type__"] = "data";

	decompileValue["_flags"] = flags;
	decompileValue["_idendifier"] = identifier;

	decompileValue["_is_restricted"] = isRestricted;
	decompileValue["_restrict_cpt"] = restrictCpt;

	// 구버전이면 하나 더 출력
	if (getArchive()->getBuildVersion() > 0x11C) { ; }
	else {
		decompileValue["_undo"] = (UINT32)undo;
	}

	// condition
	Json::Value conditionsDecompiledValue;
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		Json::Value eachDecompiledValue;
		if (!conditions[i]->decompile(eachDecompiledValue)) { return false; }
		conditionsDecompiledValue.append(eachDecompiledValue);
	}
	decompileValue["a_conditions"] = conditionsDecompiledValue;

	// action
	Json::Value actionsDecompiledValue;
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		Json::Value eachDecompiledValue;
		if (!actions[i]->decompile(eachDecompiledValue)) { return false; }
		actionsDecompiledValue.append(eachDecompiledValue);
	}
	decompileValue["b_actions"] = conditionsDecompiledValue;

	return true;
}


// json으로부터 리컴파일
bool FrameEventData::recompile(Json::Value& recompileValue)
{

	flags = (UINT16)recompileValue["_flags"].asUInt();
	identifier = (UINT16)recompileValue["_idendifier"].asUInt();

	isRestricted = recompileValue["_is_restricted"].asUInt();
	restrictCpt = recompileValue["_restrict_cpt"].asUInt();

	// condition
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		if (conditions[i] != NULL) {
			delete conditions[i];
		}
	}
	conditions.clear();
	conditions.resize(recompileValue["a_conditions"].size());
	for (UINT32 i = 0; i < (UINT32)(conditions.size()); i++) {
		conditions[i] = new FrameEventCondition();
		if (!conditions[i]->recompile(recompileValue["a_conditions"][i])) { return false; }
	}

	// action
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		if (actions[i] != NULL) {
			delete actions[i];
		}
	}
	actions.clear();
	actions.resize(recompileValue["b_actions"].size());
	for (UINT32 i = 0; i < (UINT32)(actions.size()); i++) {
		actions[i] = new FrameEventAction();
		if (!actions[i]->recompile(recompileValue["b_actions"][i])) { return false; }
	}

	return true;
}