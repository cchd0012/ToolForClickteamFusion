#pragma once

#include "../DataChunk.h"
#include "LogFont.h"

namespace CTT {

	namespace Chunk {

		class LogFontDataChunk : public DataChunk
		{
		protected:
			BaseData* allocateData() { return new LogFont(); }
		};

	}

}
