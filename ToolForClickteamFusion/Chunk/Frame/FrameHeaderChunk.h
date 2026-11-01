#pragma once

#include "../Chunk.h"
#include "FrameStructure.h"

namespace CTT {

	namespace Chunk {

		class FrameHeaderChunk : public Chunk
		{
		public:
			FrameHeaderChunk();
			~FrameHeaderChunk();
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			FrameHeader value;
		};

		vector<string> getFrameFlagProperties(UINT32 frameFlag);

	}

}
