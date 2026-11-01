#pragma once

#include "../Chunk.h"

namespace CTT {

	namespace Chunk {

		class StringChunk : public Chunk 
		{
		public:
			wstring& getValue() { return value; }
			void setValue(wstring value) { this->value = value; }
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer()
			{
				// chunk 데이터: [WCHAR 문자열] + [NULL 문자]
				WCHAR* wstr = (WCHAR*)orgBuffer;
				value = wstr;
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
				orgSize = (sizeof(WCHAR) * (UINT32)(value.size() + 1));
				orgBuffer = (BYTE*)malloc(orgSize);
				memset(orgBuffer, 0, orgSize);
				memcpy(orgBuffer, value.c_str(), (sizeof(WCHAR) * value.size()));
				return true;
			}
		private:
			wstring value;
		};

		// StringChunk를 그대로 상속받는 클래스들
		class AppNameChunk : public StringChunk { ; };
		class AppAuthorChunk : public StringChunk { ; };
		class ExtentionPathChunk : public StringChunk { ; };
		class EditorFileNameChunk : public StringChunk { ; };
		class TargetFileNameChunk : public StringChunk { ; };
		class AppDocChunk : public StringChunk { ; };
		class AboutTextChunk : public StringChunk { ; };
		class CopyrightChunk : public StringChunk { ; };
		class DemoFilePathChunk : public StringChunk { ; };
		class ObjectNameChunk : public StringChunk { ; };
		class FrameNameChunk : public StringChunk { ; };
		class FrameDemoFilePathChunk : public StringChunk { ; };
	}

}