#pragma once

#include "../Chunk.h"
#include "FrameEvent/FrameEventText.h"

namespace CTT {

	namespace Chunk {

		class FrameChunk : public Chunk 
		{
		public:
			FrameChunk();
			~FrameChunk();
			// 오직 정보 읽기용
			bool readChunkTreeInfoFromArchive(HANDLE hArchive);
			// 프레임 이벤트의 텍스트 추출 / 교체
			bool extractEventText(UINT32 frameIndex, FrameEventTextTable& table);
			bool replaceEventText(const FrameEventTextTable& table, UINT32& replacedCount);
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			vector<CTT::Chunk::Chunk*> chunkList;
		};

	}

}
