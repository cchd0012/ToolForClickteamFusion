#pragma once

#include "../../Chunk.h"
#include "FrameEventConstant.h"
#include "../../AppInfo/ShaderStructure.h"
#include "../../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;

		typedef struct _frame_event_qualifier {
			UINT16 objectInfo;
			INT16 type;
		} FrameEventQualifier;

	}

}