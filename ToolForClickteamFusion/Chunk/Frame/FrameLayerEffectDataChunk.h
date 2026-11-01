#pragma once

#include "../DataChunk.h"
#include "FrameLayerEffect.h"

namespace CTT {

	namespace Chunk {

		class FrameLayerEffectDataChunk : public DataChunk
		{
			// 미리 프레임 이펙트의 수를 넘겨받아야 처리가 가능하다
		public:
			FrameLayerEffectDataChunk() {
				frameLayerCount = 0;
			}
			void setFrameLayerCount(UINT32 frameLayerCount) {
				this->frameLayerCount = frameLayerCount;
			}
		private:
			UINT32 frameLayerCount;
		protected:
			BaseData* allocateData() { return new FrameLayerEffect(); }
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		};

	}

}
