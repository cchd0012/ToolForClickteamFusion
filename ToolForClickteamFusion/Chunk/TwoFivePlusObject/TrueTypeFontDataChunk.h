#pragma once

#include "../DataChunk.h"
#include "TrueTypeFont.h"

namespace CTT {

	namespace Chunk {

		// 데이터 출현은 보통 TrueTypeFontDataChunk -> TrueTypeFontInfoDataChunk 로 나오는거 같음

		class TrueTypeFontDataChunk : public DataChunk
		{
		public:
			bool readOnlyDataCount(HANDLE hArchive);
		protected:
			BaseData* allocateData() { return new TrueTypeFont(); }
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		};

	}

}
