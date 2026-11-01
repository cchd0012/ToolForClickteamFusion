#pragma once

#include "../Archive.h"

namespace CTT {

	namespace Archive {

		class DatArchive : public Archive 
		{
		public:
			DatArchive(wstring archivePath);
			~DatArchive();
			bool init();
		private:
			// front 헤더는 있으나 module은 Modules 폴더에 따로 존재한다
			UINT32 frontHeaderStartOffset;
		};

	}

}