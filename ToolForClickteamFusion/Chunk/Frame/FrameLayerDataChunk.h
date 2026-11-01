#pragma once

#include "../DataChunk.h"
#include "FrameLayer.h"

namespace CTT {

	namespace Chunk {

		class FrameLayerDataChunk : public DataChunk
		{
		protected:
			BaseData* allocateData() { return new FrameLayer(); }
		};

	}

}
