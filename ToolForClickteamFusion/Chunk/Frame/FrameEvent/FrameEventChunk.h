#pragma once

#include "../../Chunk.h"
#include "FrameEventBaseClass.h"

namespace CTT {

	namespace Chunk {

		class FrameEventChunk : public Chunk
		{
		public:
			FrameEventChunk();
			~FrameEventChunk();
			// 데이터 렌더링용 함수
			void renderData();
			// 텍스트 추출/교체 함수
			bool extractText(Json::Value& extractValue);
			bool replaceText(Json::Value& replaceValue);
			// 디컴파일 / 리컴파일 함수
			bool decompile(Json::Value& decompileValue);
			bool recompile(Json::Value& recompileValue);
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			// 프레임 이벤트 계열 클래스 모음
			vector<FrameEventBaseClass*> frameEventClassList;
			// 점프 클래스를 매핑하기 위한 데이터
			map<INT32, FrameEventData*> frameEventDataOffsetMapper;
		};

		// 프레임 이벤트 내 데이터 그룹 순위 반환/지정 함수
		INT32 getFrameEventDataGroupOrder();
		void setFrameEventDataGroupOrder(INT32 order);
		void increaseFrameEventDataGroupOrder();

		// 프레임 이벤트 내 데이터 순위 반환/지정 함수
		INT32 getFrameEventDataOrder();
		void setFrameEventDataOrder(INT32 order);
		void increaseFrameEventDataOrder();

	}

}
