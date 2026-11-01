#pragma once

#include "../../Chunk.h"
#include "FrameEventStructure.h"
#include "../../../Util/JsonUtil.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		class FrameEventDataGroup;
		class FrameEventData;

		class FrameEventBaseClass {
		public:
			// 버퍼로부터 읽기 / 버퍼에 기록
			virtual bool readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper) = 0;
			virtual bool writeIntoBuffer(BYTE*& bufferPnt) = 0;
			// 가리키는 FrameEventData 클래스 지정
			virtual void setPointingFrameEventData(map<INT32, FrameEventData*>& frameEventDataOffsetMapper) = 0;
			// 가리키는 FrameEventData 클래스 지정 (json과 호환 처리)
			virtual void setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList) = 0;
			// 크기 재계산 후 반환
			virtual UINT32 recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList) = 0;
			// 오프셋 업데이트
			virtual void updateOffset(INT32& accumulatingOffset) = 0;
			// 오프셋 보정
			virtual void recorrectOffset() = 0;
			// 텍스트 추출(json으로) / 교체(json으로부터)
			virtual bool extractText(Json::Value& extractValue) = 0;
			virtual bool replaceText(Json::Value& replaceValue, UINT32& replaceIndex) = 0;
			// 디컴파일(json으로) / 리컴파일(json으로부터)
			virtual bool decompile(Json::Value& decompileValue) = 0;
			virtual bool recompile(Json::Value& recompileValue) = 0;
			virtual ~FrameEventBaseClass() { ; }
		};

	}

}