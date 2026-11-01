#pragma once
#include "ArchiveConstant.h"

namespace CTT {

	namespace Archive {

#pragma pack(push, 1)

		typedef struct _archive_front_header
		{
			UINT32 firstMagic;
			UINT32 secondMagic;
			UINT32 headerSize;
			UINT32 moduleDataSize;
			UINT32 version;
			UINT32 unk1;
			UINT32 unk2;
			UINT32 moduleCount;
		} FrontHeader;

		typedef struct _archive_back_header
		{
			UINT32 firstMagic;
			UINT16 runtimeVersion;
			UINT16 runtimeSubVersion;
			UINT32 productVersion;
			UINT32 productBuildVersion;
		} BackHeader;

#pragma pack(pop)

	}

}