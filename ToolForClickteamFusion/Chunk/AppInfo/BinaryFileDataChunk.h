#pragma once

#include "../DataChunk.h"
#include "BinaryFile.h"

namespace CTT {

	namespace Chunk {

		class BinaryFileDataChunk : public DataChunk
		{
		protected:
			BaseData* allocateData() { return new BinaryFile(); }
		};

	}

}