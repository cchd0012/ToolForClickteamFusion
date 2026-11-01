#pragma once

#include "../Chunk.h"

namespace CTT {

	namespace Chunk {

		class BooleanChunk : public Chunk 
		{
		public:
			bool& getValue() { return value; }
			void setValue(bool value)  { this->value = value; }
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer()
			{
				// chunk 데이터: [bool]
				value = *(bool*)orgBuffer;
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
				orgSize = sizeof(bool);
				orgBuffer = (BYTE*)malloc(orgSize);
				*(bool*)(orgBuffer) = value;
				return true;
			}
		private:
			bool value;
		};

		// BooleanChunk를 그대로 상속받는 클래스들
		class IsDemoChunk : public BooleanChunk { ; };
		class ExeOnlyChunk : public BooleanChunk { ; };
	}

}