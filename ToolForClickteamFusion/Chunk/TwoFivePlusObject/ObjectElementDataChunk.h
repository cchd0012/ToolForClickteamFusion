#pragma once

#include "../Chunk.h"
#include "ObjectElementContainer.h"

namespace CTT {

	namespace Chunk {

		// 2.5+ 오브젝트 속성 데이터 (0x2256). ObjectElementContainer 로 요소별 압축을 풀어서 다룬다.
		// 변경하지 않은 요소는 원본 압축 바이트를 그대로 쓰므로, 아무것도 바꾸지 않으면 바이트 단위로 같은 청크가 나온다.
		class ObjectElementDataChunk : public Chunk
		{
		public:
			ObjectElementDataChunk() {}
			~ObjectElementDataChunk() {}

			// 데이터 렌더링 함수 (ImGui)
			void renderData();

			ObjectElementContainer& getContainer() { return container; }

			// 마지막으로 저장할 때 다시 계산한 0x2257 (요소 오프셋) 청크의 내용
			const ByteBuffer& getOffsetPayload() const { return offsetPayload; }

		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();

		private:
			ObjectElementContainer container;
			ByteBuffer offsetPayload;
		};

	}

}
