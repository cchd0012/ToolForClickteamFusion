#pragma once

#include "FrameEventBaseParameter.h"

namespace CTT {

	namespace Chunk {

		class FrameEventData;

		class FrameEventDataPointerParameter : public FrameEventBaseParameter {

		public:
			FrameEventDataPointerParameter();
			~FrameEventDataPointerParameter();

			// 버퍼로부터 읽기 / 버퍼에 기록
			bool readFromBuffer(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper);
			bool writeIntoBuffer(BYTE*& bufferPnt);

			// 프레임 이벤트 계열 클래스를 가리키도록 지정
			void setPointingFrameEventData(map<INT32, FrameEventData*>& frameEventDataOffsetMapper);

			// FrameEventData 클래스 지정 (json과 호환 처리)
			void setPointingFrameEventData(vector<FrameEventDataGroup*>& frameEventDataGroupList);

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
			INT32 basisOffset;
			INT32 movingDistance;								//* 오프셋 계산시 위치 저장용
			INT32 pointingOffset;								//* 오프셋 동기화용
			INT32 pointingGroupOrder;							//* json-오프셋-클래스 동기화용
			INT32 pointingDataOrder;							//* json-오프셋-클래스 동기화용
			FrameEventData* pointingFrameEventData;				//* 오프셋-클래스 동기화용
			UINT32 id;											//* 추가 id 같은건가?

		};

	}

}