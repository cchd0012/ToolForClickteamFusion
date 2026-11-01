#pragma once

#include "../DataChunk.h"
#include "Object.h"

namespace CTT {

	namespace Chunk {

		class ObjectDataChunk : public DataChunk
		{
		protected:
			BaseData* allocateData() { return new Object(); }
		};

	}

}