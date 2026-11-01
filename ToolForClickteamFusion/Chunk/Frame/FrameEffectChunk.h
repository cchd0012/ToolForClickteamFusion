#pragma once

#include "../Chunk.h"
#include "FrameStructure.h"

namespace CTT {

	namespace Chunk {

		class FrameEffectChunk : public Chunk
		{
		public:
			FrameEffectChunk();
			~FrameEffectChunk();
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			FrameEffectInstruction value;
		};

	}

}