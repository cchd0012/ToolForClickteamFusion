#include "FrameEventCondition.h"
#include "../../../Archive/Archive.h"
#include "../../../Util/CommonUtil.h"
#include "../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameEventCondition::FrameEventCondition()
{
	totalDataBufferSize = 0;
	objectType = FrameEventObjectType::UNKNOWN;
	paramType = 0;
	objectInfo = 0;
	objectInfoList = 0;
	flag1 = 0;
	flag2 = 0;
	defType = 0;
	identifier = 0;
	parameters.clear();
}


FrameEventCondition::~FrameEventCondition()
{
	for (UINT32 i = 0; i < (UINT32)parameters.size();i++) {
		if (parameters[i] != NULL) {
			delete parameters[i];
			parameters[i] = NULL;
		}
	}
	parameters.clear();
}


// 버퍼로부터 읽기
bool FrameEventCondition::readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper)
{
	// 끝부분 확인 후 포인터 옮겨주기
	totalDataBufferSize = (*(INT16*)bufferPnt);
	BYTE* endBufferPnt = bufferPnt + totalDataBufferSize;
	bufferPnt += sizeof(INT16);

	objectType = *(FrameEventObjectType*)bufferPnt;
	bufferPnt += sizeof(FrameEventObjectType);

	paramType = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	objectInfo = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	objectInfoList = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	flag1 = *(BYTE*)bufferPnt;
	bufferPnt++;

	flag2 = *(BYTE*)bufferPnt;
	bufferPnt++;

	UINT32 parameterCount = *(BYTE*)bufferPnt;
	bufferPnt++;

	defType = *(BYTE*)bufferPnt;
	bufferPnt++;

	identifier = *(UINT16*)bufferPnt;
	bufferPnt += sizeof(UINT16);

	parameters.resize(parameterCount);
	for (UINT32 i = 0; i < parameterCount;i++) {

		// 다음 위치 구해두기
		UINT16 paramDataSize = *(UINT16*)bufferPnt;
		BYTE* bufferNextPnt = bufferPnt + paramDataSize;

		// 패러미터 타입만 구하고 그걸로 읽어들이도록 변경
		FrameEventParameterType frameEventParameterType = *(FrameEventParameterType*)(bufferPnt + sizeof(UINT16));
		FrameEventBaseParameter* parameter = getParameter(frameEventParameterType);
		if (parameter != NULL) {
			if (!parameter->readFromBuffer(bufferPnt, baseBuffer, frameEventDataOffsetMapper)) {
				delete parameter;
				return false;
			}
		}

		parameters[i] = parameter;

		// 읽어들인 후 위치 이동
		bufferPnt = bufferNextPnt;

	}

	bufferPnt = endBufferPnt;
	return true;
}


// 버퍼에 기록
bool FrameEventCondition::writeIntoBuffer(BYTE*& bufferPnt)
{

	*(UINT32*)bufferPnt = totalDataBufferSize;
	bufferPnt += sizeof(INT16);

	*(FrameEventObjectType*)bufferPnt = objectType;
	bufferPnt += sizeof(FrameEventObjectType);

	*(UINT16*)bufferPnt = paramType;
	bufferPnt += sizeof(UINT16);

	*(UINT16*)bufferPnt = objectInfo;
	bufferPnt += sizeof(UINT16);

	*(UINT16*)bufferPnt = objectInfoList;
	bufferPnt += sizeof(UINT16);

	*(BYTE*)bufferPnt = flag1;
	bufferPnt++;

	*(BYTE*)bufferPnt = flag2;
	bufferPnt++;

	*(BYTE*)bufferPnt = (BYTE)(parameters.size());
	bufferPnt++;

	*(BYTE*)bufferPnt = defType;
	bufferPnt++;

	*(UINT16*)bufferPnt = identifier;
	bufferPnt += sizeof(UINT16);

	for (UINT32 i = 0; i < (UINT32)parameters.size(); i++) {
		if (!parameters[i]->writeIntoBuffer(bufferPnt)) {
			return false;
		}
	}

	return true;
}


// 프레임 이벤트 계열 클래스를 가리키도록 지정
void FrameEventCondition::setPointingFrameEventData(map<INT32, FrameEventData*>& frameDataOffsetMapper)
{
	for (UINT32 i = 0; i < (UINT32)parameters.size(); i++) {
		if (parameters[i] == NULL) { continue; }
		parameters[i]->setPointingFrameEventData(frameDataOffsetMapper);
	}
}


// FrameEventData 클래스 지정 (json과 호환 처리)
void FrameEventCondition::setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList)
{
	for (UINT32 i = 0; i < (UINT32)parameters.size(); i++) {
		if (parameters[i] == NULL) { continue; }
		parameters[i]->setPointingFrameEventData(frameEventDataGroupList);
	}
}


// 크기 재계산 후 반환
UINT32 FrameEventCondition::recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList)
{
	totalDataBufferSize = sizeof(UINT16);

	totalDataBufferSize += sizeof(FrameEventObjectType);
	totalDataBufferSize += sizeof(UINT16);
	totalDataBufferSize += sizeof(UINT16);
	totalDataBufferSize += sizeof(UINT16);
	totalDataBufferSize += sizeof(BYTE);
	totalDataBufferSize += sizeof(BYTE);
	totalDataBufferSize += sizeof(BYTE);
	totalDataBufferSize += sizeof(BYTE);
	totalDataBufferSize += sizeof(UINT16);

	for (UINT32 i = 0; i < (UINT32)parameters.size(); i++) {
		totalDataBufferSize += parameters[i]->recalculateSize(frameEventClassList);
	}

	return totalDataBufferSize;
}


// 오프셋 업데이트
void FrameEventCondition::updateOffset(INT32& accumulatingOffset)
{
	INT32 basisOffset = accumulatingOffset;
	INT32 afterAccumulatingOffset = basisOffset + totalDataBufferSize;
	for (UINT32 i = 0; i < (UINT32)parameters.size(); i++) {
		parameters[i]->updateOffset(accumulatingOffset);
	}
	accumulatingOffset = afterAccumulatingOffset;
}


// 오프셋 보정
void FrameEventCondition::recorrectOffset()
{
	for (UINT32 i = 0; i < (UINT32)parameters.size(); i++) {
		parameters[i]->recorrectOffset();
	}
}


// json으로 텍스트 추출
bool FrameEventCondition::extractText(Json::Value& extractValue)
{
	return true;
}


// json으로부터 텍스트 교체
bool FrameEventCondition::replaceText(Json::Value& replaceValue, UINT32& replaceIndex)
{
	return true;
}


// json으로 디컴파일
bool FrameEventCondition::decompile(Json::Value& decompileValue)
{
	decompileValue["__type__"] = "condition";
	return true;
}


// json으로부터 리컴파일
bool FrameEventCondition::recompile(Json::Value& recompileValue)
{
	return true;
}