#pragma once

#include "../Chunk.h"
#include "../../Util/ByteBufferUtil.h"

namespace CTT {

	namespace Chunk {

		using namespace std;
		using namespace CTT::ByteBufferUtil;

		// 2.5+ 오브젝트 헤더 하나 (0x2253 청크의 16바이트 항목)
		// 구조: [i16 핸들][i16 타입][i16 플래그][2바이트][u8 ink][7바이트]
		typedef struct _object_header_entry {
			INT16 handle;
			INT16 type;		// 0 ~ 1 은 배경, 2 이상은 ObjectCommon 을 가진 오브젝트
			INT16 flags;
			BYTE ink;
		} ObjectHeaderEntry;

		// 2.5+ 오브젝트 헤더 목록 (0x2253). 항목의 순서가 0x2256 요소의 순서와 같다.
		// 내용을 바꾸지 않으므로 읽은 원본 바이트를 그대로 보관했다가 다시 쓴다.
		class ObjectHeaderListChunk : public Chunk
		{
		public:
			ObjectHeaderListChunk() {}
			~ObjectHeaderListChunk() {}

			// 데이터 렌더링 함수 (ImGui)
			void renderData();

			const vector<ObjectHeaderEntry>& getEntries() const { return entries; }

		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();

		private:
			vector<ObjectHeaderEntry> entries;
			ByteBuffer raw;
		};

	}

}
