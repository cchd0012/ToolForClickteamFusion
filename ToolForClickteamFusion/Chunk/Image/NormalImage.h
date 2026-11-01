#pragma once

#include "BaseImage.h"
#include "../Chunk.h"
#include "../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;

		class NormalImage : public BaseImage 
		{
		public:
			NormalImage();
			~NormalImage();
			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			UINT32 getDataSize() { return sizeof(UINT32) + (sizeof(UINT32) * 2) + compressedSize; }

			UINT32 getHandle() { return handle; }
			void setHandle(UINT32 handle){ this->handle = handle; }

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		protected:
			bool deriveTranslatingData();
			bool deriveCompressedData();
		private:
			// 추후 남는 공간의 handle 위치를 넣거나, 꽉 차면 (handle최대값 + 1) 하면 될듯
			UINT32 handle;
			// 원본 사이즈는 어쩔 수 없이 관리해야 한다
			UINT32 originalSize;
		};

	}

}