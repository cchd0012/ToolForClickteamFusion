#pragma once

#include "../Archive.h"

namespace CTT {

	namespace Archive {

		// exe 아카이브와 가장 큰 차이는 안에 modules 데이터가 있느냐 없느냐
		class CcnArchive : public Archive 
		{
		public:
			CcnArchive(wstring archivePath);
			~CcnArchive();
			bool init();
		private:
		};

	}

}