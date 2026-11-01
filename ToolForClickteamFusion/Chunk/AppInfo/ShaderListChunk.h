#pragma once

#include "../DataChunk.h"
#include "Shader.h"

namespace CTT {

	namespace Chunk {

		class ShaderListChunk : public DataChunk
		{
		protected:
			BaseData* allocateData() { return new Shader(); }
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		};

	}

}
