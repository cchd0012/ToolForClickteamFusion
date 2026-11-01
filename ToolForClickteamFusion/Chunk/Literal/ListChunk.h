#pragma once

#include "../Chunk.h"

namespace CTT {

	namespace Chunk {

		template<typename T>
		class ListChunk : public Chunk
		{
		public:
			vector<T>& getValue() { return value; }
			void setValue(vector<T> &value) { this->value = value; }
			void clear() { value.clear(); }
			void insert(T val)  { value.push_back(val); }
			void insert(T val, size_t index) { value.insert(value.begin() + index, val); }
			T &get(size_t index) { return value[index]; }
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			virtual bool loadFromOrgBuffer();
			virtual bool saveIntoOrgBuffer();
			vector<T> value;
		};

		// ListChunk<UINT32>를 그대로 상속받는 클래스들
		class ImageOffsetsChunk : public ListChunk<UINT32> { ; };
		class LogFontOffsetsChunk : public ListChunk<UINT32> { ; };
		class FontOffsetsChunk : public ListChunk<UINT32> { ; };
		class SoundOffsetsChunk : public ListChunk<UINT32> { ; };
		class MusicOffsetsChunk : public ListChunk<UINT32> { ; };
		class ObjectOffsetsChunk : public ListChunk<UINT32> { ; };

		// ListChunk<string>을 그대로 상속받는 클래스들 (약간은 편차 존재)
		class AppOtherExtentionsChunk : public ListChunk<string> { ; };
		class AppGlobalStringsChunk : public ListChunk<string> { ; };

		// 아래 2개는 확실치 않음
		class ProductVersionInfoChunk : public ListChunk<UINT32> { ; };
		class ProductBuildVersionInfoChunk : public ListChunk<UINT32> { ; };

	}

}
