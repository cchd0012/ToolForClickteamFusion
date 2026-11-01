#pragma once

#include "../DataChunk.h"
#include "Sound.h"

namespace CTT {

	namespace Chunk {

		class SoundDataChunk : public DataChunk
		{
		protected:
			BaseData* allocateData()  { return new Sound(); }
		};

	}

}