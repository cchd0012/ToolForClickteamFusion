#pragma once

#include "../Common.h"

namespace CTT {

	namespace DecryptionUtil {

		void initializeDecryptionTable();
		void initializeEncryptionTable();
		void decrypt (BYTE* buffer, UINT32 bufferSize);
		void encrypt (BYTE* buffer, UINT32 bufferSize);

	}

}
