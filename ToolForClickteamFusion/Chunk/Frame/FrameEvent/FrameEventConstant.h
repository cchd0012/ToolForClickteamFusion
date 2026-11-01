#pragma once

#include "../../Chunk.h"
#include "../../AppInfo/ShaderStructure.h"
#include "../../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;

		static string frameEventHeaderMagic = "ER>>";
		static string frameEventSizeInfoMagic = "ERes";
		static string frameEventDataGroupMagic = "ERev";
		static string frameEventOptionMagic = "ERop";
		static string frameEventEndMagic = "<<ER";

	}

}