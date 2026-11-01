#pragma once

#include "../Common.h"

namespace CTT {

	namespace ZlibUtil {

		bool decode (BYTE* encodedData, UINT32 encodedSize, BYTE* &decodedData, UINT32 &decodedSize, bool isStrict = true);
		bool encode (BYTE* decodedData, UINT32 decodedSize, BYTE* &encodedData, UINT32 &encodedSize);

	}

}

