#pragma once

#include "ChunkConstant.h"

namespace CTT {

	namespace Chunk {

#pragma pack(push, 1)

		typedef struct _chunk_info 
		{
			ChunkType type;
			UINT16 flag;
			UINT32 size;
			ChunkType getType()
			{
				return type;
			}
			UINT16 getFlag()
			{
				return flag;
			}
			UINT32 getSize()
			{
				return size;
			}
		} ChunkInfo;

#pragma pack(pop)

	}

}