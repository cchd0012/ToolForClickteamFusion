#pragma once

#include "../../Chunk.h"
#include "FrameEventBaseClass.h"
#include "FrameEventCondition.h"
#include "FrameEventAction.h"
#include "FrameEventStructure.h"

namespace CTT {

	namespace Chunk {

		class FrameEventData : public FrameEventBaseClass
		{
		public:
			FrameEventData();
			~FrameEventData();

			// 버퍼로부터 읽기 / 버퍼에 기록
			bool readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper);
			bool writeIntoBuffer(BYTE*& bufferPnt);

			// 프레임 이벤트 계열 클래스를 가리키도록 지정
			void setPointingFrameEventData(map<INT32, FrameEventData*>& frameEventDataOffsetMapper);

			// FrameEventData 클래스 지정 (json과 호환 처리)
			void setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList);

			// (이 클래스 한정) 오프셋 반환
			INT32 getOffset() { return basisOffset; }

			// 크기 재계산 후 반환
			UINT32 recalculateSize(vector<FrameEventBaseClass*>& frameEventClassList);

			// 오프셋 업데이트
			void updateOffset(INT32& accumulatingOffset);

			// 오프셋 보정
			void recorrectOffset();

			// 텍스트 추출(json으로) / 교체(json으로부터)
			bool extractText(Json::Value& extractValue);
			bool replaceText(Json::Value& replaceValue, UINT32& replaceIndex);

			// 디컴파일(json으로) / 리컴파일(json으로부터)
			bool decompile(Json::Value& decompileValue);
			bool recompile(Json::Value& recompileValue);

		private:
			// 실제 변수들
			INT16 totalDataBufferSize;
			UINT16 flags;
			// 구버전/신버전 공통 변수들
			UINT16 identifier;			// 신버전에서는 line으로도 쓰인다
			UINT32 isRestricted;
			UINT32 restrictCpt;
			// 구버전 추가 변수
			UINT16 undo;
			// 실제 데이터
			vector<FrameEventCondition*> conditions;
			vector<FrameEventAction*> actions;
			// 연동용 변수들
			INT32 basisOffset;									//* 현재 이 클래스에 대응되는 오프셋
			INT32 groupOrder;									//* json-오프셋-클래스 동기화용
			INT32 dataOrder;									//* json-오프셋-클래스 동기화용
		};

	}

}