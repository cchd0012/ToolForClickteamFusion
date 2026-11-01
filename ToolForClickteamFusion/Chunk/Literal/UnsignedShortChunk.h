#pragma once

#include "../Chunk.h"

namespace CTT {

	namespace Chunk {

		class UnsignedShortChunk : public Chunk
		{
		public:
			UINT16& getValue() { return value; }
			void setValue(UINT16 value) { this->value = value; }
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer() {
				// chunk 데이터: [4바이트 정수]
				value = *(UINT16*)orgBuffer;
				free(orgBuffer);
				orgBuffer = NULL;
				return true;
			}
			bool saveIntoOrgBuffer()
			{
				if (orgBuffer != NULL) {
					free(orgBuffer);
					orgBuffer = NULL;
				}
				orgSize = sizeof(UINT16);
				orgBuffer = (BYTE*)malloc(orgSize);
				*(UINT16*)(orgBuffer) = value;
				return true;
			}
		private:
			UINT16 value = 0;
		};

		// UnsignedShortChunk를 그대로 상속받는 클래스들
		class FrameRandomSeedChunk : public UnsignedShortChunk { ; };
	}

}