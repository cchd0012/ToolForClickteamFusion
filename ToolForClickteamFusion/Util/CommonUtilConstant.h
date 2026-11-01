#pragma once

#include "../Common.h"

namespace CTT {

	namespace CommonUtil {

		enum class CodePage {
			CP_KOR,
			CP_JAP,
			CP_ANSI,
			CP_UTF_8
		};

		enum class FSOType {
			FSO_FILE,
			FSO_DIRECTORY,
			FSO_NONE
		};

	}

}
