#pragma once

#include "../Chunk.h"

namespace CTT {

	namespace Chunk {

		class IntegerChunk : public Chunk 
		{
		public:
			UINT32& getValue() { return value; }
			void setValue(UINT32 value)  { this->value = value; }
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer() {
				// chunk 데이터: [4바이트 정수]
				value = *(UINT32*)orgBuffer;
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
				orgSize = sizeof(UINT32);
				orgBuffer = (BYTE*)malloc(orgSize);
				*(UINT32*)(orgBuffer) = value;
				return true;
			}
		private:
			UINT32 value;
		};

		// IntegerChunk를 그대로 상속받는 클래스들
		class ProtectionChunk : public IntegerChunk { ; };
		class AppLocaleIdChunk : public IntegerChunk { ; };
		class AppCodepageChunk : public IntegerChunk { ; };
		class FrameIndexChunk : public IntegerChunk { ; };
		class FrameMvtTimerBaseChunk : public IntegerChunk { ; };
	}

}