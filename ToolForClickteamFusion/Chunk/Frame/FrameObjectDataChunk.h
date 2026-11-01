#pragma once

#include "../DataChunk.h"
#include "FrameObject.h"

namespace CTT {

	namespace Chunk {

		class FrameObjectDataChunk : public DataChunk
		{
		protected:
			BaseData* allocateData() { return new FrameObject(); }
		};

	}

}
