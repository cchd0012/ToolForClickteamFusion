#pragma once

#include "../Literal/ListChunk.h"

namespace CTT {

	namespace Chunk {

		class ObjectNamesChunk : public ListChunk<string>
		{
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		};

	}

}