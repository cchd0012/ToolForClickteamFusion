#pragma once

#include "BaseImage.h"
#include "../Chunk.h"
#include "../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::CommonUtil;

		// TwoFivePlusImage는 [AdditionalImageInfo] + [CommonImageInfo] 구조로 되어있다
		typedef struct _additional_image_info
		{
			UINT32 handle;		// 추후 남는 공간의 handle 위치를 넣거나, 꽉 차면 (handle최대값 + 1) 하면 될듯
			UINT32 unknown1;	// 일단은 0xFFFFFFFF면 될듯
		} AdditionalImageInfo;

		class TwoFivePlusImage : public BaseImage
		{
		public:
			TwoFivePlusImage();
			~TwoFivePlusImage();
			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			UINT32 getDataSize() { return sizeof(AdditionalImageInfo) + sizeof(CommonImageInfo) + sizeof(UINT32) + compressedSize; }

			UINT32 getHandle() { return additional.handle; }
			void setHandle(UINT32 handle) { additional.handle = handle; }

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		protected:
			// 2.5+에서 추가되는 데이터..보다는, 개편하면서 데이터 위치를 옮긴 것 같다
			AdditionalImageInfo additional;
			bool deriveTranslatingData();
			bool deriveCompressedData();
		};

	}

}