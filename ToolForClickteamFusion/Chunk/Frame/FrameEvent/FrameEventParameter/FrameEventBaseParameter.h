#pragma once

#include "../../../Chunk.h"
#include "../FrameEventBaseClass.h"
#include "../FrameEventStructure.h"
#include "FrameEventParameterConstant.h"

namespace CTT {

	namespace Chunk {

		class FrameEventBaseParameter : public FrameEventBaseClass {
		protected:
			UINT16 totalDataBufferSize;
			FrameEventParameterType type;
			//virtual void readFromBufferEachParam(BYTE*& bufferPnt, BYTE*& baseBuffer, map<INT32, FrameEventData*>& frameEventDataOffsetMapper) = 0;
		};

		// 패러미터 클래스 획득 함수
		FrameEventBaseParameter* getParameter(FrameEventParameterType frameEventParameterType);

	}

}