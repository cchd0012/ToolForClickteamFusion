#pragma once

#include "../Chunk.h"
#include "FrameStructure.h"

namespace CTT {

	namespace Chunk {

		class FrameVirtualRectChunk : public Chunk
		{
		public:
			FrameVirtualRectChunk();
			~FrameVirtualRectChunk();
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			FrameVirtualRect value;
		};

	}

}
