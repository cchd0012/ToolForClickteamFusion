#pragma once

#include "../DataChunk.h"
#include "../../Archive/Archive.h"
#include "NormalImage.h"
#include "TwoFivePlusImage.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::Archive;

		class ImageDataChunk : public DataChunk
		{
		protected:
			BaseData* allocateData() 
			{
				BaseImage* img = NULL;
				// 이건 나중에 제보를 받으면서 확인하던가 해야겠음
				if (getArchive()->isTwoPointFiveImage()) {
					img = new TwoFivePlusImage();
				}
				else {
					img = new NormalImage();
				}
				return img;
			}
		};

	}

}