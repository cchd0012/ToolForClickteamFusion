#pragma once

#include "../Chunk.h"
#include "FrameStructure.h"

namespace CTT {

	namespace Chunk {

		class FrameTransitionChunk : public Chunk
		{
		public:
			FrameTransitionChunk();
			~FrameTransitionChunk();
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			FrameTransitionInfo value;
		};

		// FrameTransitionChunk를 그대로 상속받는 클래스들
		// FrameFadeInFrameChunk과 FrameFadeOutFrameChunk는 호출하는게 없던데 이게 무슨 역할을 하는지 모르겠음
		class FrameFadeInChunk : public FrameTransitionChunk { ; };
		class FrameFadeOutChunk : public FrameTransitionChunk { ; };
		class FrameFadeInFrameChunk : public FrameTransitionChunk { ; };
		class FrameFadeOutFrameChunk : public FrameTransitionChunk { ; };

	}

}