#pragma once

#include "../DataChunk.h"
#include "TrueTypeFontInfo.h"

namespace CTT {

	namespace Chunk {

		// 데이터 출현은 보통 TrueTypeFontDataChunk -> TrueTypeFontInfoDataChunk 로 나오는거 같음

		class TrueTypeFontInfoDataChunk : public DataChunk
		{
		public:
			bool readOnlyDataCount(HANDLE hArchive);
		protected:
			BaseData* allocateData() { return new TrueTypeFontInfo(); }
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		};

	}

}
