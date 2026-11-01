#pragma once

#include "../Chunk.h"

namespace CTT {

	namespace Chunk {

		class FrameChunk : public Chunk 
		{
		public:
			FrameChunk();
			~FrameChunk();
			// 오직 정보 읽기용
			bool readChunkTreeInfoFromArchive(HANDLE hArchive);
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
