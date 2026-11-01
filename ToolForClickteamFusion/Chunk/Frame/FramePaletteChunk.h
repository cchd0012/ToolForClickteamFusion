#pragma once

#include "../Chunk.h"
#include "../../Util/CommonStructure.h"
#include "../../ImGUI/Headers.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;

		class FramePaletteChunk : public Chunk
		{
		public:
			FramePaletteChunk();
			~FramePaletteChunk();
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			// 크기는 0x101로 고정인 것 같다
			vector<Color> value;
			ImVec4 colorPalette[0x101];
		};

	}

}
